#include "max6675.h"
#include "reflow_pins.h"
#include "sw_spi.h"
#include "GPIO_Init.h"
#include "os_timer.h"

// Software SPI instance for thermocouple
static _SW_SPI tc_spi;

// Reading state
static TC_Reading lastReading = { .temperature = 0.0f, .status = TC_NOT_READY, .timestamp = 0 };
static float filterBuffer[TC_FILTER_SAMPLES];
static uint8_t filterIndex = 0;
static bool filterFilled = false;
static uint32_t lastReadTime = 0;

void MAX6675_Init(void)
{
  // Configure software SPI for MAX6675
  // MAX6675 uses SPI Mode 0 (CPOL=0, CPHA=0), 16-bit reads
  SW_SPI_Config(&tc_spi, _SPI_MODE0, 16,
                TC_CS_PIN,
                TC_SCK_PIN,
                TC_MISO_PIN,
                TC_MOSI_PIN);

  // CS starts high (inactive)
  SW_SPI_CS_Set(&tc_spi, 1);

  // Initialize filter buffer
  for (int i = 0; i < TC_FILTER_SAMPLES; i++)
  {
    filterBuffer[i] = 0.0f;
  }

  lastReading.status = TC_NOT_READY;
  lastReadTime = 0;
}

uint16_t MAX6675_ReadRaw(void)
{
  // Pull CS low to begin read
  SW_SPI_CS_Set(&tc_spi, 0);

  // Read 16 bits (MAX6675 clocks out data on falling edge of SCK)
  uint16_t raw = SW_SPI_Read_Write(&tc_spi, 0x0000);

  // Pull CS high -- this also starts the next conversion (~220ms)
  SW_SPI_CS_Set(&tc_spi, 1);

  return raw;
}

TC_Reading MAX6675_Read(void)
{
  TC_Reading reading;
  reading.timestamp = OS_GetTimeMs();

  uint16_t raw = MAX6675_ReadRaw();

  // Bit 2: thermocouple open flag (1 = open / not connected)
  if (raw & 0x04)
  {
    reading.temperature = 0.0f;
    reading.status = TC_OPEN;
    lastReading = reading;
    return reading;
  }

  // Bits 15-3: 12-bit temperature value, MSB first
  // Each bit = 0.25C, so divide by 4 to get degrees C
  float temp = (float)((raw >> 3) & 0x0FFF) * 0.25f;

  // Range validation
  if (temp < TC_MIN_VALID_TEMP || temp > TC_MAX_VALID_TEMP)
  {
    reading.temperature = temp;
    reading.status = TC_RANGE_ERROR;
    lastReading = reading;
    return reading;
  }

  // Update moving average filter
  filterBuffer[filterIndex] = temp;
  filterIndex = (filterIndex + 1) % TC_FILTER_SAMPLES;
  if (filterIndex == 0)
    filterFilled = true;

  reading.temperature = temp;
  reading.status = TC_OK;
  lastReading = reading;

  return reading;
}

TC_Reading MAX6675_GetLastReading(void)
{
  return lastReading;
}

float MAX6675_GetFilteredTemp(void)
{
  uint8_t count = filterFilled ? TC_FILTER_SAMPLES : filterIndex;
  if (count == 0)
    return lastReading.temperature;

  float sum = 0.0f;
  for (uint8_t i = 0; i < count; i++)
  {
    sum += filterBuffer[i];
  }
  return sum / (float)count;
}

bool MAX6675_IsConnected(void)
{
  return (lastReading.status == TC_OK);
}

void MAX6675_Update(void)
{
  uint32_t now = OS_GetTimeMs();

  // MAX6675 needs ~220ms between conversions. We read at TC_READ_INTERVAL_MS.
  if ((now - lastReadTime) >= TC_READ_INTERVAL_MS)
  {
    MAX6675_Read();
    lastReadTime = now;
  }
}
