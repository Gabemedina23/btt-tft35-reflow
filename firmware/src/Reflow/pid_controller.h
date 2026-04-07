#ifndef _PID_CONTROLLER_H_
#define _PID_CONTROLLER_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

typedef struct {
  // Tuning parameters
  float kp;               // Proportional gain
  float ki;               // Integral gain
  float kd;               // Derivative gain

  // Output limits
  float outputMin;         // Minimum output (0 = 0% duty)
  float outputMax;         // Maximum output (100 = 100% duty)

  // Internal state
  float integral;          // Accumulated integral term
  float prevError;         // Previous error for derivative calculation
  float prevTemp;          // Previous temperature for derivative-on-measurement
  float output;            // Current output value
  bool  firstRun;          // True on first iteration (skip derivative)

  // Feed-forward for ramp control
  float feedForwardGain;   // Kff -- power per (C/s) of desired ramp rate
} PID_Controller;

// Initialize PID controller with gains and output limits
void PID_Init(PID_Controller *pid, float kp, float ki, float kd,
              float outputMin, float outputMax);

// Set PID gains (for tuning without reinitializing)
void PID_SetGains(PID_Controller *pid, float kp, float ki, float kd);

// Set feed-forward gain for ramp rate compensation
void PID_SetFeedForward(PID_Controller *pid, float kff);

// Compute PID output
// currentTemp: current oven temperature (C)
// targetTemp:  desired temperature (C)
// dt:          time since last call (seconds)
// Returns: duty cycle 0-100
float PID_Compute(PID_Controller *pid, float currentTemp, float targetTemp, float dt);

// Compute PID output with ramp rate feed-forward
// desiredRampRate: target ramp rate in C/s (positive = heating, negative = cooling)
float PID_ComputeWithRamp(PID_Controller *pid, float currentTemp, float targetTemp,
                          float desiredRampRate, float dt);

// Reset PID state (call when switching stages or restarting)
void PID_Reset(PID_Controller *pid);

// Get current output value without computing
float PID_GetOutput(PID_Controller *pid);

#ifdef __cplusplus
}
#endif

#endif // _PID_CONTROLLER_H_
