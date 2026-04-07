#ifndef _MAX6675_H_
#define _MAX6675_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

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

// Initialize the MAX6675 software SPI interface
void MAX6675_Init(void);

// Read raw 16-bit value from MAX6675
// Returns raw SPI data. Bit 2 = open thermocouple flag.
uint16_t MAX6675_ReadRaw(void);

// Read temperature with filtering and validation
// Returns the filtered temperature and updates status
TC_Reading MAX6675_Read(void);

// Get the most recent valid reading without triggering a new SPI read
// Useful for the display/UI to read without blocking
TC_Reading MAX6675_GetLastReading(void);

// Get the filtered temperature (moving average)
float MAX6675_GetFilteredTemp(void);

// Check if sensor is connected and reading valid data
bool MAX6675_IsConnected(void);

// Called periodically from the main loop or timer ISR
// Handles read timing (MAX6675 needs 220ms between conversions)
void MAX6675_Update(void);

#ifdef __cplusplus
}
#endif

#endif // _MAX6675_H_
