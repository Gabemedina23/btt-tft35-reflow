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

static void Fan_Init(void)
{
  GPIO_InitSet(FAN_PIN, MGPIO_MODE_OUT_PP, 0);
  GPIO_SetLevel(FAN_PIN, FAN_ACTIVE_LOW ? 1 : 0);  // Start OFF
}

static void Fan_Set(bool on)
{
  if (FAN_ACTIVE_LOW)
    GPIO_SetLevel(FAN_PIN, on ? 0 : 1);
  else
    GPIO_SetLevel(FAN_PIN, on ? 1 : 0);
}

// =============================================================================
// SSR Software PWM
//
// The SSR is switched at a 1-second period. dutyCycle (0-100) determines
// what fraction of the period the SSR is ON. This provides proportional
// power control suitable for zero-crossing SSRs.
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
  // Check thermocouple connection
  if (!MAX6675_IsConnected())
  {
    ctrl.error = REFLOW_ERR_SENSOR_OPEN;
    return false;
  }

  // Check absolute max temperature
  if (ctrl.currentTemp > ctrl.profile.maxTemp)
  {
    ctrl.error = REFLOW_ERR_THERMAL_RUNAWAY;
    return false;
  }

  // Check thermal runaway (temp way above target)
  if (ctrl.state != REFLOW_COOL && ctrl.state != REFLOW_COMPLETE)
  {
    if (ctrl.currentTemp > ctrl.targetTemp + SAFETY_THERMAL_RUNAWAY_C)
    {
      ctrl.error = REFLOW_ERR_THERMAL_RUNAWAY;
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

  // Reset PID integral when entering a new stage to avoid windup carryover
  PID_Reset(&ctrl.pid);
  PID_SetGains(&ctrl.pid, ctrl.profile.pidKp, ctrl.profile.pidKi, ctrl.profile.pidKd);
}

static void AdvanceStage(void)
{
  uint8_t nextStage = ctrl.currentStage + 1;

  if (nextStage >= ctrl.profile.numStages)
  {
    // Profile complete
    ctrl.state = REFLOW_COMPLETE;
    ctrl.dutyCycle = 0.0f;
    SSR_Set(false);
    Reflow_SetFan(false);
    return;
  }

  EnterStage(nextStage);

  // Determine state from stage name/characteristics
  const ReflowStage *stage = &ctrl.profile.stages[nextStage];
  if (stage->rampRate < 0.0f)
    ctrl.state = REFLOW_COOL;
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

static void RecordTemperature(float temp)
{
  ctrl.history.samples[ctrl.history.head] = temp;
  ctrl.history.head = (ctrl.history.head + 1) % TEMP_HISTORY_SIZE;
  if (ctrl.history.count < TEMP_HISTORY_SIZE)
    ctrl.history.count++;
}

// =============================================================================
// Public API
// =============================================================================

void Reflow_Init(void)
{
  memset(&ctrl, 0, sizeof(ctrl));
  ctrl.state = REFLOW_IDLE;
  ctrl.error = REFLOW_ERR_NONE;

  // Initialize hardware
  MAX6675_Init();
  SSR_Init();
  Fan_Init();

  // Initialize PID with default values (will be overwritten when profile starts)
  PID_Init(&ctrl.pid, 200.0f, 5.0f, 1000.0f, PID_OUTPUT_MIN, PID_OUTPUT_MAX);
  PID_SetFeedForward(&ctrl.pid, 10.0f);  // Default feed-forward gain
}

void Reflow_Update(void)
{
  // Always update thermocouple readings
  MAX6675_Update();
  ctrl.currentTemp = MAX6675_GetFilteredTemp();

  // SSR PWM runs continuously based on current duty cycle
  if (ctrl.state != REFLOW_IDLE && ctrl.state != REFLOW_COMPLETE &&
      ctrl.state != REFLOW_ERROR && ctrl.state != REFLOW_ABORTED)
  {
    SSR_UpdatePWM(ctrl.dutyCycle);
  }
  else
  {
    SSR_Set(false);  // Ensure heater is off in non-active states
  }

  // State machine only needs to run at PID rate (1 Hz)
  uint32_t now = OS_GetTimeMs();
  if ((now - ctrl.lastPidTime) < PID_UPDATE_INTERVAL_MS)
    return;

  float dt = (float)(now - ctrl.lastPidTime) / 1000.0f;
  ctrl.lastPidTime = now;

  // Record temperature for graph (1 sample per PID cycle = 1/s)
  if (ctrl.state >= REFLOW_PREHEAT && ctrl.state <= REFLOW_COOL)
  {
    RecordTemperature(ctrl.currentTemp);
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
      Reflow_SetFan(true);  // Turn on fan to cool down safely
      break;

    case REFLOW_PREHEAT:
    case REFLOW_RAMP:
    {
      // Safety check
      if (!CheckSafety()) { ctrl.state = REFLOW_ERROR; break; }

      const ReflowStage *stage = &ctrl.profile.stages[ctrl.currentStage];

      // During preheat/ramp: use bang-bang until within 20C of target, then PID
      if (ctrl.currentTemp < stage->targetTemp - 20.0f)
      {
        ctrl.dutyCycle = PID_OUTPUT_MAX;  // Full power
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
          // This stage has a hold -- transition to soak/peak
          ctrl.state = (stage->rampRate == 0.0f) ? REFLOW_PEAK : REFLOW_SOAK;
          ctrl.stageStartTime = OS_GetTimeMs();  // Reset for hold timing
        }
        else
        {
          AdvanceStage();
        }
      }

      ctrl.fanOn = false;
      break;
    }

    case REFLOW_SOAK:
    case REFLOW_PEAK:
    {
      if (!CheckSafety()) { ctrl.state = REFLOW_ERROR; break; }

      const ReflowStage *stage = &ctrl.profile.stages[ctrl.currentStage];

      // PID to hold at target temperature
      ctrl.dutyCycle = PID_Compute(&ctrl.pid, ctrl.currentTemp, stage->targetTemp, dt);

      // Check if hold time has elapsed
      uint32_t holdElapsed = (OS_GetTimeMs() - ctrl.stageStartTime) / 1000;
      if (holdElapsed >= stage->holdTime)
      {
        AdvanceStage();
      }

      ctrl.fanOn = false;
      break;
    }

    case REFLOW_COOL:
    {
      // Heater off, fan on
      ctrl.dutyCycle = 0.0f;
      SSR_Set(false);
      Reflow_SetFan(true);
      ctrl.fanOn = true;

      const ReflowStage *stage = &ctrl.profile.stages[ctrl.currentStage];

      // Check if we've cooled to target
      if (ctrl.currentTemp <= stage->targetTemp)
      {
        AdvanceStage();  // Will set state to REFLOW_COMPLETE if last stage
      }
      break;
    }

    default:
      break;
  }
}

void Reflow_Start(const ReflowProfile *profile)
{
  // Copy profile so we can modify PID values during tuning
  memcpy(&ctrl.profile, profile, sizeof(ReflowProfile));

  // Reset state
  ctrl.state = REFLOW_PREHEAT;
  ctrl.error = REFLOW_ERR_NONE;
  ctrl.profileStartTime = OS_GetTimeMs();
  ctrl.lastPidTime = OS_GetTimeMs();
  ctrl.dutyCycle = 0.0f;
  ctrl.fanOn = false;

  // Clear history
  ctrl.history.head = 0;
  ctrl.history.count = 0;

  // Initialize PID with profile parameters
  PID_Init(&ctrl.pid, profile->pidKp, profile->pidKi, profile->pidKd,
           PID_OUTPUT_MIN, PID_OUTPUT_MAX);
  PID_SetFeedForward(&ctrl.pid, 10.0f);

  // Enter first stage
  EnterStage(0);
}

void Reflow_Abort(void)
{
  ctrl.state = REFLOW_ABORTED;
  ctrl.dutyCycle = 0.0f;
  SSR_Set(false);
  Reflow_SetFan(true);  // Cool down after abort
}

void Reflow_Reset(void)
{
  ctrl.state = REFLOW_IDLE;
  ctrl.error = REFLOW_ERR_NONE;
  ctrl.dutyCycle = 0.0f;
  ctrl.fanOn = false;
  SSR_Set(false);
  Reflow_SetFan(false);
}

void Reflow_SetSSR(float dutyCycle)
{
  ctrl.dutyCycle = dutyCycle;
}

void Reflow_SetFan(bool on)
{
  ctrl.fanOn = on;
  Fan_Set(on);
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
