#ifndef _REFLOW_PINS_H_
#define _REFLOW_PINS_H_

// =============================================================================
// Reflow Oven Pin Definitions for BTT TFT35-E3 V3.0.1 (GD32F205 / STM32F207)
//
// All pins use EXISTING headers on the board -- no trace cutting required.
// =============================================================================

#include "GPIO_Init.h"  // for PA0, PB10, PC12, etc. pin macros and GPIO API

// --- Dual MAX6675 Thermocouples (Software SPI, shared SCK/MISO) ---
//
// TC1: Board thermocouple (next to PCB being reflowed) — used for PID control
// TC2: Ambient/chamber thermocouple — used for display and safety
//
// Both share SCK and MISO lines (SPI is point-to-point read, CS selects which chip)
// SCK and MISO on UART3 header, CS pins on FIL-DET and UART4 headers

#define TC_SCK_PIN    PB10    // UART3 TX — shared by both MAX6675s
#define TC_MISO_PIN   PB11    // UART3 RX — shared by both MAX6675s
#define TC_MOSI_PIN   PB10    // dummy, MAX6675 is read-only

#define TC1_CS_PIN    PA15    // FIL-DET signal — board thermocouple (PID control)
#define TC2_CS_PIN    PC11    // UART4 RX — ambient/chamber thermocouple

// --- SSR Control (Heating Elements) ---
// Using PS-ON header (bottom edge): [PC12] [GND] [3.3V]
// Direct GPIO — SSR-40DA rated 3-32VDC input, 3.3V is at minimum threshold
#define SSR_PIN       PC12    // PS-ON signal pin

// SSR wired direct to GPIO:
//   PS-ON 3.3V pin → SSR DC+ (pin 3)
//   PS-ON PC12 pin → SSR DC- (pin 4)
//   PC12 LOW = current flows = SSR ON
//   PC12 HIGH = no current = SSR OFF
#define SSR_ACTIVE_LOW  1     // Direct GPIO wiring (no transistor)

// --- Buzzer ---
// On-board buzzer — already defined in pin_TFT35_V3_0.h as BUZZER_PIN = PD13

// --- Optional: ESP32 WiFi Bridge ---
// Using WIFI header: PA9 (TX1), PA10 (RX1) = USART1
#define WIFI_SERIAL_PORT  _USART1

// --- Temperature reading config ---
#define TC_READ_INTERVAL_MS    250    // Read thermocouple every 250ms (MAX6675 needs 220ms conversion)
#define TC_FILTER_SAMPLES      4      // Moving average filter depth
#define TC_MAX_VALID_TEMP      350.0f // Readings above this are sensor error
#define TC_MIN_VALID_TEMP      0.0f   // Readings below this are sensor error (open thermocouple)

// --- PID control config ---
#define PID_UPDATE_INTERVAL_MS 500    // PID loop runs at 2 Hz
#define PID_OUTPUT_MIN         0.0f   // Minimum duty cycle (0%)
#define PID_OUTPUT_MAX         100.0f // Maximum duty cycle (100%)
#define PID_SSR_PERIOD_MS      1000   // SSR PWM period (1 second = 1 Hz on/off cycling)

// --- Safety limits ---
#define SAFETY_MAX_TEMP_DEFAULT   280.0f  // Absolute max before emergency shutdown
#define SAFETY_THERMAL_RUNAWAY_C  20.0f   // Degrees above target = thermal runaway
#define SAFETY_STAGE_TIMEOUT_MULT 3.0f    // Stage timeout = expected_time * multiplier

#endif // _REFLOW_PINS_H_
