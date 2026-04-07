#ifndef _REFLOW_PINS_H_
#define _REFLOW_PINS_H_

// =============================================================================
// Reflow Oven Pin Definitions for BTT TFT35-E3 V3.0.1 (GD32F205 / STM32F207)
//
// All pins use EXISTING headers on the board -- no trace cutting required.
// =============================================================================

#include "GPIO_Init.h"  // for PA0, PB10, PC12, etc. pin macros and GPIO API

// --- MAX6675 Thermocouple (Software SPI) ---
// Using UART3 header (bottom edge) + FIL-DET header
#define TC_SCK_PIN   PB10    // UART3 TX pad
#define TC_MISO_PIN  PB11    // UART3 RX pad
#define TC_CS_PIN    PA15    // FIL-DET signal pin

// MAX6675 has no MOSI -- it's read-only. We define a dummy for the SW_SPI struct.
// Use any unused pin; the SW_SPI init will configure it but we never write.
#define TC_MOSI_PIN  PB10    // dummy, not used by MAX6675

// --- SSR Control (Heating Elements) ---
// Using PS-ON header (bottom edge): [PC12] [GND] [3.3V]
#define SSR_PIN      PC12    // PS-ON signal pin

// SSR logic: 1 = heater ON, 0 = heater OFF
// If using NPN transistor (recommended): GPIO HIGH -> transistor ON -> SSR ON
// If direct GPIO: GPIO LOW -> SSR ON (inverted) -- set SSR_ACTIVE_LOW 1
#define SSR_ACTIVE_LOW  0    // Set to 1 if wired without transistor (direct GPIO)

// --- Cooling Fan ---
// Using UART4 header: [GND] [RX4] [TX4/PA0]
#define FAN_PIN      PA0     // UART4 TX pad -- TIM2_CH1 or TIM5_CH1 for PWM

// Fan logic: 1 = fan ON, 0 = fan OFF (via external MOSFET)
#define FAN_ACTIVE_LOW  0

// --- Buzzer ---
// On-board buzzer (shared with EXP1 pin 10)
// Already defined in pin_TFT35_V3_0.h as BUZZER_PIN = PD13
// We reuse the existing buzzer infrastructure

// --- Optional: Second SSR for dual-zone control ---
// Could use an EXP1 pin (e.g., PB15 = LCD_EN = EXP1 pin 8)
// #define SSR2_PIN     PB15

// --- Optional: ESP32 WiFi Bridge ---
// Using WIFI header: PA9 (TX1), PA10 (RX1) = USART1
// Already defined as SERIAL_PORT_2 = _USART1 in pin_TFT35_V3_0.h
#define WIFI_SERIAL_PORT  _USART1

// --- Temperature reading config ---
#define TC_READ_INTERVAL_MS    250    // Read thermocouple every 250ms (MAX6675 needs 220ms conversion)
#define TC_FILTER_SAMPLES      4      // Moving average filter depth
#define TC_MAX_VALID_TEMP      350.0f // Readings above this are sensor error
#define TC_MIN_VALID_TEMP      0.0f   // Readings below this are sensor error (open thermocouple)

// --- PID control config ---
#define PID_UPDATE_INTERVAL_MS 1000   // PID loop runs at 1 Hz
#define PID_OUTPUT_MIN         0.0f   // Minimum duty cycle (0%)
#define PID_OUTPUT_MAX         100.0f // Maximum duty cycle (100%)
#define PID_SSR_PERIOD_MS      1000   // SSR PWM period (1 second = 1 Hz on/off cycling)

// --- Safety limits ---
#define SAFETY_MAX_TEMP_DEFAULT   280.0f  // Absolute max before emergency shutdown
#define SAFETY_THERMAL_RUNAWAY_C  20.0f   // Degrees above target = thermal runaway
#define SAFETY_STAGE_TIMEOUT_MULT 3.0f    // Stage timeout = expected_time * multiplier

#endif // _REFLOW_PINS_H_
