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

// Initialize both MAX6675 software SPI interfaces
void MAX6675_Init(void);

// Read temperature from a specific sensor with filtering and validation
TC_Reading MAX6675_Read(TC_Sensor sensor);

// Get the most recent valid reading without triggering a new SPI read
TC_Reading MAX6675_GetLastReading(TC_Sensor sensor);

// Get the filtered temperature (moving average) for a sensor
float MAX6675_GetFilteredTemp(TC_Sensor sensor);

// Check if a sensor is connected and reading valid data
bool MAX6675_IsConnected(TC_Sensor sensor);

// Called periodically from the main loop
// Handles read timing for both sensors (alternating reads)
void MAX6675_Update(void);

#ifdef __cplusplus
}
#endif

#endif // _MAX6675_H_
