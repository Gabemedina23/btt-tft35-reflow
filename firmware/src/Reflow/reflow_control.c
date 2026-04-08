#include "includes.h"
#include "reflow_control.h"
#include "reflow_pins.h"
#include "max6675.h"
#include <math.h>

// =============================================================================
// Static state
// =============================================================================

static ReflowController ctrl;
static uint32_t ssrPeriodStart = 0;  // For SSR software PWM timing

// =============================================================================
// Hardware I/O
// =============================================================================

static void SSR_Init(void)
{
  GPIO_InitSet(SSR_PIN, MGPIO_MODE_OUT_PP, 0);
  GPIO_SetLevel(SSR_PIN, SSR_ACTIVE_LOW ? 1 : 0);  // Start OFF
}

static void SSR_Set(bool on)
{
  if (SSR_ACTIVE_LOW)
    GPIO_SetLevel(SSR_PIN, on ? 0 : 1);
  else
    GPIO_SetLevel(SSR_PIN, on ? 1 : 0);
}

// =============================================================================
// SSR Software PWM
// =============================================================================

static void SSR_UpdatePWM(float dutyCycle)
{
  uint32_t now = OS_GetTimeMs();
  uint32_t elapsed = now - ssrPeriodStart;

  if (elapsed >= PID_SSR_PERIOD_MS)
  {
    ssrPeriodStart = now;
    elapsed = 0;
  }

  uint32_t onTime = (uint32_t)(dutyCycle * PID_SSR_PERIOD_MS / 100.0f);

  SSR_Set(elapsed < onTime);
}

// =============================================================================
// Safety checks
// =============================================================================

static bool CheckSafety(void)
{
  char logMsg[80];

  // Check board thermocouple connection
  if (!MAX6675_IsConnected(TC_BOARD))
  {
    ctrl.error = REFLOW_ERR_SENSOR_OPEN;
    ReflowLog_Event("ERROR: Board thermocouple disconnected");
    ReflowLog_Stop();
    return false;
  }

  // Check absolute max temperature
  if (ctrl.currentTemp > ctrl.profile.maxTemp)
  {
    ctrl.error = REFLOW_ERR_THERMAL_RUNAWAY;
    sprintf(logMsg, "ERROR: Max temp exceeded (%.1fC > %.1fC max)",
            (double)ctrl.currentTemp, (double)ctrl.profile.maxTemp);
    ReflowLog_Event(logMsg);
    ReflowLog_Stop();
    return false;
  }

  // Check thermal runaway (temp way above target)
  if (ctrl.state != REFLOW_COOL && ctrl.state != REFLOW_COMPLETE)
  {
    if (ctrl.currentTemp > ctrl.targetTemp + SAFETY_THERMAL_RUNAWAY_C)
    {
      ctrl.error = REFLOW_ERR_THERMAL_RUNAWAY;
      sprintf(logMsg, "ERROR: Thermal runaway (%.1fC, target %.1fC, limit +%.0fC)",
              (double)ctrl.currentTemp, (double)ctrl.targetTemp,
              (double)SAFETY_THERMAL_RUNAWAY_C);
      ReflowLog_Event(logMsg);
      ReflowLog_Stop();
      return false;
    }
  }

  // Check stage timeout
  if (ctrl.state >= REFLOW_PREHEAT && ctrl.state <= REFLOW_COOL)
  {
    uint32_t stageElapsed = (OS_GetTimeMs() - ctrl.stageStartTime) / 1000;
    uint16_t expectedDuration = Profile_GetStageDuration(
      &ctrl.profile.stages[ctrl.currentStage], ctrl.currentTemp);

    if (expectedDuration > 0)
    {
      uint32_t timeout = (uint32_t)(expectedDuration * ctrl.profile.stageTimeoutMult);
      if (stageElapsed > timeout && timeout > 0)
      {
        ctrl.error = REFLOW_ERR_STAGE_TIMEOUT;
        sprintf(logMsg, "ERROR: Stage timeout (%lus > %lus limit, stage %d)",
                (unsigned long)stageElapsed, (unsigned long)timeout, ctrl.currentStage);
        ReflowLog_Event(logMsg);
        ReflowLog_Stop();
        return false;
      }
    }
  }

  return true;
}

// =============================================================================
// State machine helpers
// =============================================================================

static void EnterStage(uint8_t stageIndex)
{
  ctrl.currentStage = stageIndex;
  ctrl.stageStartTime = OS_GetTimeMs();
  ctrl.targetTemp = ctrl.profile.stages[stageIndex].targetTemp;

  PID_Reset(&ctrl.pid);
  PID_SetGains(&ctrl.pid, ctrl.profile.pidKp, ctrl.profile.pidKi, ctrl.profile.pidKd);
}

static void AdvanceStage(void)
{
  uint8_t nextStage = ctrl.currentStage + 1;

  if (nextStage >= ctrl.profile.numStages)
  {
    ctrl.state = REFLOW_COMPLETE;
    ctrl.dutyCycle = 0.0f;
    SSR_Set(false);
    ReflowLog_Event("Profile complete");
    ReflowLog_Stop();
    return;
  }

  EnterStage(nextStage);

  const ReflowStage *stage = &ctrl.profile.stages[nextStage];
  if (stage->rampRate < 0.0f)
  {
    ctrl.state = REFLOW_COOL;
    Buzzer_Play(SOUND_NOTIFY);  // Beep to signal "open the door!"
  }
  else if (stage->rampRate == 0.0f && stage->holdTime > 0)
    ctrl.state = REFLOW_PEAK;
  else if (stage->holdTime > 0)
    ctrl.state = REFLOW_SOAK;
  else
    ctrl.state = REFLOW_RAMP;
}

// =============================================================================
// Record temperature for graphing
// =============================================================================

static void RecordTemperature(TempHistory *hist, float temp)
{
  hist->samples[hist->head] = temp;
  hist->head = (hist->head + 1) % TEMP_HISTORY_SIZE;
  if (hist->count < TEMP_HISTORY_SIZE)
    hist->count++;
}

// =============================================================================
// Public API
// =============================================================================

void Reflow_Init(void)
{
  memset(&ctrl, 0, sizeof(ctrl));
  ctrl.state = REFLOW_IDLE;
  ctrl.error = REFLOW_ERR_NONE;

  MAX6675_Init();
  SSR_Init();

  PID_Init(&ctrl.pid, 200.0f, 5.0f, 1000.0f, PID_OUTPUT_MIN, PID_OUTPUT_MAX);
  PID_SetFeedForward(&ctrl.pid, 10.0f);
}

void Reflow_Update(void)
{
  // Always update thermocouple readings (both sensors)
  MAX6675_Update();
  ctrl.currentTemp = MAX6675_GetFilteredTemp(TC_BOARD);
  ctrl.ambientTemp = MAX6675_GetFilteredTemp(TC_AMBIENT);

  // SSR PWM runs continuously based on current duty cycle
  if (ctrl.state != REFLOW_IDLE && ctrl.state != REFLOW_COMPLETE &&
      ctrl.state != REFLOW_ERROR && ctrl.state != REFLOW_ABORTED)
  {
    SSR_UpdatePWM(ctrl.dutyCycle);
  }
  else
  {
    SSR_Set(false);
  }

  // State machine runs at PID rate (1 Hz)
  uint32_t now = OS_GetTimeMs();
  if ((now - ctrl.lastPidTime) < PID_UPDATE_INTERVAL_MS)
    return;

  float dt = (float)(now - ctrl.lastPidTime) / 1000.0f;
  ctrl.lastPidTime = now;

  // Record temperatures for graph (~1 sample per second)
  static bool recordToggle = false;
  recordToggle = !recordToggle;
  if (recordToggle && ctrl.state >= REFLOW_PREHEAT && ctrl.state <= REFLOW_COOL)
  {
    RecordTemperature(&ctrl.history, ctrl.currentTemp);
    RecordTemperature(&ctrl.ambientHistory, ctrl.ambientTemp);

    // Log to SD card
    const char *stageName = (ctrl.currentStage < ctrl.profile.numStages) ?
                            ctrl.profile.stages[ctrl.currentStage].name : "";
    ReflowLog_Write(ctrl.currentTemp, ctrl.ambientTemp, ctrl.targetTemp,
                    ctrl.dutyCycle, Reflow_GetStateName(ctrl.state), stageName);
  }

  // State machine
  switch (ctrl.state)
  {
    case REFLOW_IDLE:
    case REFLOW_COMPLETE:
    case REFLOW_ABORTED:
      ctrl.dutyCycle = 0.0f;
      break;

    case REFLOW_ERROR:
      ctrl.dutyCycle = 0.0f;
      SSR_Set(false);
      break;

    case REFLOW_PREHEAT:
    case REFLOW_RAMP:
    {
      if (!CheckSafety()) { ctrl.state = REFLOW_ERROR; break; }

      const ReflowStage *stage = &ctrl.profile.stages[ctrl.currentStage];

      // Full power until within 10C of target, then PID takes over
      if (ctrl.currentTemp < stage->targetTemp - 10.0f)
      {
        ctrl.dutyCycle = PID_OUTPUT_MAX;
        // Keep PID primed so transition is smooth
        PID_Reset(&ctrl.pid);
        ctrl.pid.output = PID_OUTPUT_MAX;
      }
      else
      {
        ctrl.dutyCycle = PID_ComputeWithRamp(&ctrl.pid, ctrl.currentTemp,
                                              stage->targetTemp, stage->rampRate, dt);
      }

      // Check if we've reached the target temperature
      if (ctrl.currentTemp >= stage->targetTemp - 1.0f)
      {
        if (stage->holdTime > 0)
        {
          ctrl.state = (stage->rampRate == 0.0f) ? REFLOW_PEAK : REFLOW_SOAK;
          ctrl.stageStartTime = OS_GetTimeMs();
        }
        else
        {
          AdvanceStage();
        }
      }
      break;
    }

    case REFLOW_SOAK:
    case REFLOW_PEAK:
    {
      if (!CheckSafety()) { ctrl.state = REFLOW_ERROR; break; }

      const ReflowStage *stage = &ctrl.profile.stages[ctrl.currentStage];

      ctrl.dutyCycle = PID_Compute(&ctrl.pid, ctrl.currentTemp, stage->targetTemp, dt);

      uint32_t holdElapsed = (OS_GetTimeMs() - ctrl.stageStartTime) / 1000;
      if (holdElapsed >= stage->holdTime)
      {
        AdvanceStage();
      }
      break;
    }

    case REFLOW_COOL:
    {
      // Heater off, door open for passive cooling (no fan)
      ctrl.dutyCycle = 0.0f;
      SSR_Set(false);

      // Beep every 30 seconds to remind user to open the door
      {
        static uint32_t lastBeepTime = 0;
        if ((now - lastBeepTime) >= 30000)
        {
          Buzzer_Play(SOUND_NOTIFY);
          lastBeepTime = now;
        }
      }

      const ReflowStage *stage = &ctrl.profile.stages[ctrl.currentStage];

      if (ctrl.currentTemp <= stage->targetTemp)
      {
        AdvanceStage();
      }
      break;
    }

    default:
      break;
  }
}

void Reflow_Start(const ReflowProfile *profile)
{
  memcpy(&ctrl.profile, profile, sizeof(ReflowProfile));

  ctrl.state = REFLOW_PREHEAT;
  ctrl.error = REFLOW_ERR_NONE;
  ctrl.profileStartTime = OS_GetTimeMs();
  ctrl.lastPidTime = OS_GetTimeMs();
  ctrl.dutyCycle = PID_OUTPUT_MAX;  // Start at full power immediately

  // Start SD card logging
  ReflowLog_Start("reflow");
  ReflowLog_Event(ctrl.profile.name);

  // Clear history for both sensors
  ctrl.history.head = 0;
  ctrl.history.count = 0;
  ctrl.ambientHistory.head = 0;
  ctrl.ambientHistory.count = 0;

  PID_Init(&ctrl.pid, profile->pidKp, profile->pidKi, profile->pidKd,
           PID_OUTPUT_MIN, PID_OUTPUT_MAX);
  PID_SetFeedForward(&ctrl.pid, 10.0f);

  EnterStage(0);
}

void Reflow_Abort(void)
{
  ctrl.state = REFLOW_ABORTED;
  ctrl.dutyCycle = 0.0f;
  SSR_Set(false);
  ReflowLog_Event("Aborted by user");
  ReflowLog_Stop();
}

void Reflow_Reset(void)
{
  ctrl.state = REFLOW_IDLE;
  ctrl.error = REFLOW_ERR_NONE;
  ctrl.dutyCycle = 0.0f;
  SSR_Set(false);
}

void Reflow_SetSSR(float dutyCycle)
{
  ctrl.dutyCycle = dutyCycle;
}

const ReflowController * Reflow_GetState(void)
{
  return &ctrl;
}

ReflowState Reflow_GetStatus(void)
{
  return ctrl.state;
}

uint32_t Reflow_GetElapsedTime(void)
{
  if (ctrl.profileStartTime == 0) return 0;
  return (OS_GetTimeMs() - ctrl.profileStartTime) / 1000;
}

uint32_t Reflow_GetRemainingTime(void)
{
  uint32_t total = Profile_GetExpectedDuration(&ctrl.profile, 25.0f);
  uint32_t elapsed = Reflow_GetElapsedTime();
  return (elapsed < total) ? (total - elapsed) : 0;
}

const TempHistory * Reflow_GetHistory(void)
{
  return &ctrl.history;
}

const char * Reflow_GetStateName(ReflowState state)
{
  switch (state)
  {
    case REFLOW_IDLE:     return "Idle";
    case REFLOW_PREHEAT:  return "Preheat";
    case REFLOW_SOAK:     return "Soak";
    case REFLOW_RAMP:     return "Ramp";
    case REFLOW_PEAK:     return "Peak";
    case REFLOW_COOL:     return "Cooling";
    case REFLOW_COMPLETE: return "Complete";
    case REFLOW_ERROR:    return "ERROR";
    case REFLOW_ABORTED:  return "Aborted";
    default:              return "Unknown";
  }
}

const char * Reflow_GetErrorName(ReflowError error)
{
  switch (error)
  {
    case REFLOW_ERR_NONE:             return "None";
    case REFLOW_ERR_SENSOR_OPEN:      return "Thermocouple disconnected";
    case REFLOW_ERR_SENSOR_RANGE:     return "Sensor out of range";
    case REFLOW_ERR_THERMAL_RUNAWAY:  return "Thermal runaway!";
    case REFLOW_ERR_STAGE_TIMEOUT:    return "Stage timeout";
    case REFLOW_ERR_HEATER_FAULT:     return "Heater not responding";
    default:                          return "Unknown error";
  }
}
