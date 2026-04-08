#ifndef _REFLOW_LOG_H_
#define _REFLOW_LOG_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

// Log file format: CSV with headers
// timestamp_ms,elapsed_s,board_temp,ambient_temp,target_temp,duty_cycle,state,stage
//
// Files are named: RFLOW_XX.CSV (00-99, auto-incrementing)
// Stored in root of SD card for easy access

// Initialize logging system
void ReflowLog_Init(void);

// Start a new log file for a session (reflow, burn-in, or auto-tune)
// sessionName: e.g., "reflow", "burnin", "autotune"
// Returns true if file was opened successfully
bool ReflowLog_Start(const char *sessionName);

// Write a data row to the log
// Called from the control loop at each PID cycle
void ReflowLog_Write(float boardTemp, float ambientTemp, float targetTemp,
                     float dutyCycle, const char *state, const char *stage);

// Write a text note/event to the log (e.g., "Stage change: Preheat -> Soak")
void ReflowLog_Event(const char *event);

// Flush and close the current log file
void ReflowLog_Stop(void);

// Check if logging is active
bool ReflowLog_IsActive(void);

// Get the current log filename (for display)
const char * ReflowLog_GetFilename(void);

#ifdef __cplusplus
}
#endif

#endif // _REFLOW_LOG_H_
