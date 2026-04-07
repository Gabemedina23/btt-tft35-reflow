#include "pid_controller.h"

void PID_Init(PID_Controller *pid, float kp, float ki, float kd,
              float outputMin, float outputMax)
{
  pid->kp = kp;
  pid->ki = ki;
  pid->kd = kd;
  pid->outputMin = outputMin;
  pid->outputMax = outputMax;
  pid->feedForwardGain = 0.0f;

  PID_Reset(pid);
}

void PID_SetGains(PID_Controller *pid, float kp, float ki, float kd)
{
  pid->kp = kp;
  pid->ki = ki;
  pid->kd = kd;
}

void PID_SetFeedForward(PID_Controller *pid, float kff)
{
  pid->feedForwardGain = kff;
}

// Clamp a value between min and max
static float clamp(float value, float min, float max)
{
  if (value < min) return min;
  if (value > max) return max;
  return value;
}

float PID_Compute(PID_Controller *pid, float currentTemp, float targetTemp, float dt)
{
  return PID_ComputeWithRamp(pid, currentTemp, targetTemp, 0.0f, dt);
}

float PID_ComputeWithRamp(PID_Controller *pid, float currentTemp, float targetTemp,
                          float desiredRampRate, float dt)
{
  if (dt <= 0.0f)
    return pid->output;

  float error = targetTemp - currentTemp;

  // --- Proportional term ---
  float pTerm = pid->kp * error;

  // --- Integral term with anti-windup ---
  pid->integral += error * dt;

  // Anti-windup: clamp integral so it can't drive output beyond limits
  float iTermMax = (pid->outputMax - pTerm) / pid->ki;
  float iTermMin = (pid->outputMin - pTerm) / pid->ki;
  if (pid->ki > 0.0f)
    pid->integral = clamp(pid->integral, iTermMin, iTermMax);

  float iTerm = pid->ki * pid->integral;

  // --- Derivative term (derivative-on-measurement to avoid setpoint kick) ---
  float dTerm = 0.0f;
  if (!pid->firstRun)
  {
    // Use rate of change of temperature, not error
    // Negative sign: if temp is rising, we want LESS output
    float dTemp = (currentTemp - pid->prevTemp) / dt;
    dTerm = -pid->kd * dTemp;
  }

  // --- Feed-forward term for ramp rate ---
  float ffTerm = pid->feedForwardGain * desiredRampRate;

  // --- Combine and clamp ---
  pid->output = clamp(pTerm + iTerm + dTerm + ffTerm, pid->outputMin, pid->outputMax);

  // --- Save state for next iteration ---
  pid->prevError = error;
  pid->prevTemp = currentTemp;
  pid->firstRun = false;

  return pid->output;
}

void PID_Reset(PID_Controller *pid)
{
  pid->integral = 0.0f;
  pid->prevError = 0.0f;
  pid->prevTemp = 0.0f;
  pid->output = 0.0f;
  pid->firstRun = true;
}

float PID_GetOutput(PID_Controller *pid)
{
  return pid->output;
}
