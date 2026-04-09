#ifndef _REFLOW_CONTROL_H_
#define _REFLOW_CONTROL_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include "reflow_profile.h"
#include "pid_controller.h"

// Reflow oven operating states
typedef enum {
  REFLOW_IDLE = 0,        // Oven off, waiting for user
  REFLOW_WARMUP,          // SSR on, waiting for board temp to start rising (+5C)
  REFLOW_PREHEAT,         // Ramping to preheat temperature
  REFLOW_SOAK,            // Soaking at temperature
  REFLOW_RAMP,            // Ramping to reflow peak
  REFLOW_PEAK,            // Holding at peak temperature
  REFLOW_COOL,            // Cooling down (heater off, open door)
  REFLOW_COMPLETE,        // Profile finished successfully
  REFLOW_ERROR,           // Error state (thermal runaway, timeout, sensor fault)
  REFLOW_ABORTED,         // User cancelled
} ReflowState;

// Error codes
typedef enum {
  REFLOW_ERR_NONE = 0,
  REFLOW_ERR_SENSOR_OPEN,       // Thermocouple disconnected
  REFLOW_ERR_SENSOR_RANGE,      // Temperature reading out of range
  REFLOW_ERR_THERMAL_RUNAWAY,   // Temperature exceeded safe limit
  REFLOW_ERR_STAGE_TIMEOUT,     // Stage took too long
  REFLOW_ERR_HEATER_FAULT,      // Heater not responding (no temp rise after 60s)
  REFLOW_ERR_TOO_HOT_TO_START,  // Oven too hot to begin a new run
  REFLOW_ERR_MAX_RUNTIME,       // Absolute max runtime exceeded
} ReflowError;

// Temperature history for graphing
#define TEMP_HISTORY_SIZE  360    // 6 minutes at 1 sample/second

typedef struct {
  float    samples[TEMP_HISTORY_SIZE];
  uint16_t head;                  // Next write position
  uint16_t count;                 // Number of valid samples
} TempHistory;

// Main reflow controller state
typedef struct {
  ReflowState     state;
  ReflowError     error;
  ReflowProfile   profile;        // Active profile (copied so user can modify)
  PID_Controller  pid;
  uint8_t         currentStage;   // Index into profile.stages[]
  float           currentTemp;    // Board thermocouple reading (C) — used for PID
  float           ambientTemp;    // Ambient/chamber thermocouple reading (C)
  float           targetTemp;     // Current target for display
  float           dutyCycle;      // SSR duty cycle (0-100%)
  uint32_t        stageStartTime; // Tick when current stage began (ms)
  float           stageStartTemp; // Board temp when current stage began
  float           warmupStartTemp; // Board temp when warmup began (for heater fault detection)
  uint32_t        profileStartTime; // Tick when profile started (ms)
  uint32_t        lastPidTime;    // Tick of last PID computation
  TempHistory     history;        // Temperature log for graphing (board TC)
  TempHistory     ambientHistory; // Temperature log for graphing (ambient TC)
} ReflowController;

// --- Lifecycle ---

// Initialize the reflow controller (call once at startup)
void Reflow_Init(void);

// Main update function -- call from main loop at ~100Hz or faster
// Handles thermocouple reading, PID computation, SSR timing, state machine
void Reflow_Update(void);

// --- Control ---

// Start a reflow profile
void Reflow_Start(const ReflowProfile *profile);

// Abort the current reflow (emergency stop)
void Reflow_Abort(void);

// Reset to idle state (clear error, ready for next run)
void Reflow_Reset(void);

// --- SSR hardware control ---

// Set SSR output (called by PID loop)
// dutyCycle: 0.0 = always off, 100.0 = always on
void Reflow_SetSSR(float dutyCycle);

// --- Status queries ---

// Get current controller state
const ReflowController * Reflow_GetState(void);

// Get current reflow state enum
ReflowState Reflow_GetStatus(void);

// Get elapsed time since profile start (seconds)
uint32_t Reflow_GetElapsedTime(void);

// Get expected remaining time (seconds, estimate)
uint32_t Reflow_GetRemainingTime(void);

// Get temperature history for graphing
const TempHistory * Reflow_GetHistory(void);

// Get human-readable state name
const char * Reflow_GetStateName(ReflowState state);

// Get human-readable error description
const char * Reflow_GetErrorName(ReflowError error);

#ifdef __cplusplus
}
#endif

#endif // _REFLOW_CONTROL_H_
