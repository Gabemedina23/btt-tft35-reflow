#ifndef _MAX6675_H_
#define _MAX6675_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

// Thermocouple sensor index
typedef enum {
  TC_BOARD = 0,     // TC1: next to the PCB being reflowed (used for PID control)
  TC_AMBIENT,       // TC2: ambient/chamber temperature
  TC_COUNT
} TC_Sensor;

// MAX6675 status flags
typedef enum {
  TC_OK = 0,              // Valid reading
  TC_OPEN,                // Thermocouple not connected (open circuit)
  TC_RANGE_ERROR,         // Reading out of valid range
  TC_NOT_READY,           // Conversion not complete / not yet read
} TC_Status;

// Thermocouple reading result
typedef struct {
  float    temperature;   // Degrees Celsius (0.25C resolution)
  TC_Status status;       // Sensor status
  uint32_t timestamp;     // Tick count when reading was taken
} TC_Reading;

// =============================================================================
// Calibration
// =============================================================================

#define TC_CAL_POINTS  6   // Number of calibration points

typedef struct {
  float sensorC;    // Raw sensor reading (°C)
  float actualC;    // Actual temperature from reference thermometer (°C)
} TC_CalPoint;

typedef struct {
  TC_CalPoint points[TC_CAL_POINTS];
  uint8_t     numPoints;     // How many points are populated (0 = uncalibrated)
  bool        enabled;       // Whether calibration correction is applied
} TC_Calibration;

// Initialize both MAX6675 software SPI interfaces
void MAX6675_Init(void);

// Read temperature from a specific sensor with filtering and validation
TC_Reading MAX6675_Read(TC_Sensor sensor);

// Get the most recent valid reading without triggering a new SPI read
TC_Reading MAX6675_GetLastReading(TC_Sensor sensor);

// Get the filtered temperature (moving average) for a sensor
// Returns calibration-corrected value if calibration is enabled
float MAX6675_GetFilteredTemp(TC_Sensor sensor);

// Get the RAW filtered temperature (no calibration correction)
float MAX6675_GetRawFilteredTemp(TC_Sensor sensor);

// Check if a sensor is connected and reading valid data
bool MAX6675_IsConnected(TC_Sensor sensor);

// Called periodically from the main loop
// Handles read timing for both sensors (alternating reads)
void MAX6675_Update(void);

// Apply piecewise linear calibration correction to a raw temperature
float MAX6675_CalibrateTemp(float rawC);

// Set calibration data (called after calibration procedure)
void MAX6675_SetCalibration(const TC_Calibration *cal);

// Get current calibration data
const TC_Calibration * MAX6675_GetCalibration(void);

// Save calibration to SD card (TCAL.DAT)
bool MAX6675_SaveCalibration(void);

// Load calibration from SD card (TCAL.DAT)
bool MAX6675_LoadCalibration(void);

#ifdef __cplusplus
}
#endif

#endif // _MAX6675_H_
