#include "includes.h"
#include "pid_autotune.h"
#include "reflow_pins.h"
#include <math.h>

void Autotune_Init(AutotuneContext *ctx, float targetTemp)
{
  ctx->state = AUTOTUNE_IDLE;
  ctx->targetTemp = targetTemp;
  ctx->hysteresis = 5.0f;        // 5°C deadband
  ctx->dutyCycleHigh = 100.0f;   // Full power when ON
  ctx->dutyCycleLow = 0.0f;      // Off when OFF
  ctx->cycleCount = 0;
  ctx->minCycles = 4;            // Need at least 4 oscillations
  ctx->peakCount = 0;
  ctx->valleyCount = 0;
  ctx->lastAboveTarget = false;
  ctx->currentPeak = 0.0f;
  ctx->currentValley = 9999.0f;
  ctx->Ku = 0.0f;
  ctx->Tu = 0.0f;
  ctx->resultKp = 0.0f;
  ctx->resultKi = 0.0f;
  ctx->resultKd = 0.0f;
  ctx->startTime = 0;
  ctx->maxDuration = 600000;     // 10 minutes max
}

void Autotune_Start(AutotuneContext *ctx)
{
  ctx->state = AUTOTUNE_HEATING;
  ctx->cycleCount = 0;
  ctx->peakCount = 0;
  ctx->valleyCount = 0;
  ctx->lastAboveTarget = false;
  ctx->currentPeak = 0.0f;
  ctx->currentValley = 9999.0f;
  ctx->startTime = OS_GetTimeMs();
}

float Autotune_Update(AutotuneContext *ctx, float currentTemp)
{
  if (ctx->state == AUTOTUNE_IDLE || ctx->state == AUTOTUNE_COMPLETE ||
      ctx->state == AUTOTUNE_ERROR)
    return 0.0f;

  // Timeout check
  if ((OS_GetTimeMs() - ctx->startTime) > ctx->maxDuration)
  {
    ctx->state = AUTOTUNE_ERROR;
    return 0.0f;
  }

  float output = 0.0f;

  switch (ctx->state)
  {
    case AUTOTUNE_HEATING:
      // Initial ramp — full power until we first reach target
      output = ctx->dutyCycleHigh;
      if (currentTemp >= ctx->targetTemp)
      {
        ctx->state = AUTOTUNE_RELAY_OFF;
        ctx->lastAboveTarget = true;
        ctx->currentPeak = currentTemp;
        ctx->currentValley = 9999.0f;
      }
      break;

    case AUTOTUNE_RELAY_ON:
      // Heater ON — waiting for temp to rise above target + hysteresis
      output = ctx->dutyCycleHigh;

      // Track current peak
      if (currentTemp > ctx->currentPeak)
        ctx->currentPeak = currentTemp;

      if (currentTemp >= ctx->targetTemp + ctx->hysteresis)
      {
        // We've overshot — record peak and switch to cooling
        if (ctx->peakCount < AUTOTUNE_MAX_CYCLES)
        {
          ctx->peakTemps[ctx->peakCount] = ctx->currentPeak;
          ctx->peakTimes[ctx->peakCount] = OS_GetTimeMs();
          ctx->peakCount++;
        }

        ctx->state = AUTOTUNE_RELAY_OFF;
        ctx->lastAboveTarget = true;
        ctx->currentValley = 9999.0f;
        ctx->cycleCount++;

        // Check if we have enough cycles
        if (ctx->cycleCount >= ctx->minCycles && ctx->peakCount >= 2 && ctx->valleyCount >= 2)
        {
          // Compute results
          float avgPeriod = 0.0f;
          uint8_t periodCount = 0;

          // Period = time between consecutive peaks
          for (uint8_t i = 1; i < ctx->peakCount; i++)
          {
            avgPeriod += (float)(ctx->peakTimes[i] - ctx->peakTimes[i - 1]) / 1000.0f;
            periodCount++;
          }
          if (periodCount > 0)
            avgPeriod /= periodCount;

          // Amplitude = average of (peak - valley) pairs
          float avgAmplitude = 0.0f;
          uint8_t ampCount = 0;
          uint8_t minPV = (ctx->peakCount < ctx->valleyCount) ? ctx->peakCount : ctx->valleyCount;
          for (uint8_t i = 0; i < minPV; i++)
          {
            avgAmplitude += ctx->peakTemps[i] - ctx->valleyTemps[i];
            ampCount++;
          }
          if (ampCount > 0)
            avgAmplitude /= ampCount;

          if (avgPeriod > 0.0f && avgAmplitude > 0.0f)
          {
            // Ziegler-Nichols relay method
            // Ku = (4 * d) / (pi * a)
            // where d = relay output range, a = oscillation amplitude
            float d = ctx->dutyCycleHigh - ctx->dutyCycleLow;
            ctx->Ku = (4.0f * d) / (3.14159f * avgAmplitude);
            ctx->Tu = avgPeriod;

            // Ziegler-Nichols PID formula (some overshoot variant)
            // Kp = 0.33 * Ku  (conservative, less overshoot than classic 0.6)
            // Ki = Kp / (Tu / 2)
            // Kd = Kp * (Tu / 8)
            ctx->resultKp = 0.33f * ctx->Ku;
            ctx->resultKi = ctx->resultKp / (ctx->Tu / 2.0f);
            ctx->resultKd = ctx->resultKp * (ctx->Tu / 8.0f);

            ctx->state = AUTOTUNE_COMPLETE;
            return 0.0f;
          }
        }
      }
      break;

    case AUTOTUNE_RELAY_OFF:
      // Heater OFF — waiting for temp to drop below target - hysteresis
      output = ctx->dutyCycleLow;

      // Track current valley
      if (currentTemp < ctx->currentValley)
        ctx->currentValley = currentTemp;

      if (currentTemp <= ctx->targetTemp - ctx->hysteresis)
      {
        // We've undershot — record valley and switch to heating
        if (ctx->valleyCount < AUTOTUNE_MAX_CYCLES)
        {
          ctx->valleyTemps[ctx->valleyCount] = ctx->currentValley;
          ctx->valleyTimes[ctx->valleyCount] = OS_GetTimeMs();
          ctx->valleyCount++;
        }

        ctx->state = AUTOTUNE_RELAY_ON;
        ctx->lastAboveTarget = false;
        ctx->currentPeak = 0.0f;
      }
      break;

    default:
      break;
  }

  return output;
}

bool Autotune_IsRunning(AutotuneContext *ctx)
{
  return (ctx->state == AUTOTUNE_HEATING ||
          ctx->state == AUTOTUNE_RELAY_ON ||
          ctx->state == AUTOTUNE_RELAY_OFF);
}

void Autotune_GetResults(AutotuneContext *ctx, float *kp, float *ki, float *kd)
{
  *kp = ctx->resultKp;
  *ki = ctx->resultKi;
  *kd = ctx->resultKd;
}

void Autotune_Abort(AutotuneContext *ctx)
{
  ctx->state = AUTOTUNE_IDLE;
}
