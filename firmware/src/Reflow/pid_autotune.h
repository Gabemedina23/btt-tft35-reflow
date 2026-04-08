#ifndef _PID_AUTOTUNE_H_
#define _PID_AUTOTUNE_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

// =============================================================================
// PID Auto-Tune using Relay Feedback (Ziegler-Nichols)
//
// The auto-tuner oscillates the oven around a target temperature using
// bang-bang control (full on / full off) and measures the resulting
// oscillation period and amplitude. From these, it calculates optimal
// PID gains using the Ziegler-Nichols relay method.
//
// Process:
// 1. Heat to target temp with full power
// 2. When temp crosses target, turn heater off
// 3. When temp drops below target - hysteresis, turn heater on
// 4. Repeat for several cycles (typically 4-6)
// 5. Measure average period and amplitude of oscillation
// 6. Calculate Ku (ultimate gain) and Tu (ultimate period)
// 7. Apply Ziegler-Nichols formulas for PID gains
// =============================================================================

typedef enum {
  AUTOTUNE_IDLE = 0,
  AUTOTUNE_HEATING,     // Initial ramp to target
  AUTOTUNE_RELAY_ON,    // Relay on (heating), waiting for overshoot
  AUTOTUNE_RELAY_OFF,   // Relay off (cooling), waiting for undershoot
  AUTOTUNE_COMPLETE,    // Done, results available
  AUTOTUNE_ERROR,       // Failed (timeout, sensor error)
} AutotuneState;

#define AUTOTUNE_MAX_CYCLES 8

typedef struct {
  AutotuneState state;
  float         targetTemp;       // Temperature to oscillate around
  float         hysteresis;       // Deadband (degrees above/below target)
  float         dutyCycleHigh;    // Power when relay is ON (default 100%)
  float         dutyCycleLow;     // Power when relay is OFF (default 0%)

  // Measurement
  uint8_t       cycleCount;       // Number of completed oscillation cycles
  uint8_t       minCycles;        // Minimum cycles before computing (default 4)
  float         peakTemps[AUTOTUNE_MAX_CYCLES];   // Peak temps per cycle
  float         valleyTemps[AUTOTUNE_MAX_CYCLES];  // Valley temps per cycle
  uint32_t      peakTimes[AUTOTUNE_MAX_CYCLES];    // Time of each peak (ms)
  uint32_t      valleyTimes[AUTOTUNE_MAX_CYCLES];  // Time of each valley (ms)
  uint8_t       peakCount;
  uint8_t       valleyCount;
  bool          lastAboveTarget;  // Was the last reading above target?
  float         currentPeak;      // Tracking current peak
  float         currentValley;    // Tracking current valley

  // Results
  float         Ku;               // Ultimate gain
  float         Tu;               // Ultimate period (seconds)
  float         resultKp;
  float         resultKi;
  float         resultKd;

  // Timing
  uint32_t      startTime;
  uint32_t      maxDuration;      // Timeout (ms), default 10 minutes
} AutotuneContext;

// Initialize auto-tune context with defaults
void Autotune_Init(AutotuneContext *ctx, float targetTemp);

// Start the auto-tune process
void Autotune_Start(AutotuneContext *ctx);

// Update — call at PID rate. Returns the duty cycle to apply.
float Autotune_Update(AutotuneContext *ctx, float currentTemp);

// Check if auto-tune is still running
bool Autotune_IsRunning(AutotuneContext *ctx);

// Get results (only valid after AUTOTUNE_COMPLETE)
void Autotune_GetResults(AutotuneContext *ctx, float *kp, float *ki, float *kd);

// Abort auto-tune
void Autotune_Abort(AutotuneContext *ctx);

#ifdef __cplusplus
}
#endif

#endif // _PID_AUTOTUNE_H_
