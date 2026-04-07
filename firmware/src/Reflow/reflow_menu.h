#ifndef _REFLOW_MENU_H_
#define _REFLOW_MENU_H_

#ifdef __cplusplus
extern "C" {
#endif

// =============================================================================
// Reflow Oven Menu Screens
//
// These functions integrate with the BTT TouchScreen firmware menu system.
// Each menu function follows the BTT pattern: called repeatedly in the main
// loop while active, handles its own drawing and input.
//
// To integrate into the BTT firmware:
// 1. Add #include "Reflow/reflow_menu.h" to Menu/includes.h
// 2. Add menu entries in the main menu (or replace printer-specific menus)
// 3. Call Reflow_Init() from main.c initialization
// 4. Call Reflow_Update() from the main loop
// =============================================================================

// Main reflow menu -- entry point from the BTT main menu
// Shows: Start Reflow, Profile Select, PID Tuning, Settings
void menuReflowMain(void);

// Active reflow screen -- shows live temperature graph, stage, controls
// Displayed while a reflow profile is running
void menuReflowActive(void);

// Profile selection menu -- list available profiles from SD + built-ins
void menuReflowProfiles(void);

// Profile editor -- modify stages, temperatures, times
void menuReflowProfileEdit(void);

// PID tuning menu -- adjust Kp, Ki, Kd with live feedback
void menuReflowPIDTune(void);

// Temperature monitor -- passive mode, just shows current temperature
// Useful for oven testing and thermocouple verification
void menuReflowMonitor(void);

// Settings menu -- SSR polarity, fan config, safety limits, WiFi
void menuReflowSettings(void);

#ifdef __cplusplus
}
#endif

#endif // _REFLOW_MENU_H_
