#include "includes.h"
#include "max6675.h"
#include "reflow_pins.h"

// =============================================================================
// Dual MAX6675 driver
//
// Both sensors share SCK (PB10) and MISO (PB11) lines.
// Each has its own CS pin: TC1=PA15 (board), TC2=PA0 (ambient).
// We alternate reads between sensors each update cycle.
// =============================================================================

// Software SPI instance (shared bus, CS managed manually)
static _SW_SPI tc_spi;

// Per-sensor state
typedef struct {
  uint16_t    csPin;
  TC_Reading  lastReading;
  float       filterBuffer[TC_FILTER_SAMPLES];
  uint8_t     filterIndex;
  bool        filterFilled;
} TC_SensorState;

static TC_SensorState sensors[TC_COUNT];
static uint32_t lastReadTime = 0;
static uint8_t  nextSensor = 0;  // alternates between TC_BOARD and TC_AMBIENT

// Calibration data (applied to board sensor only)
static TC_Calibration calData = { .numPoints = 0, .enabled = false };

void MAX6675_Init(void)
{
  // Configure shared SPI bus using TC1's CS pin initially
  // We'll manage CS pins manually for each sensor
  SW_SPI_Config(&tc_spi, _SPI_MODE0, 16,
                TC1_CS_PIN,    // CS (will be overridden per-read)
                TC_SCK_PIN,
                TC_MISO_PIN,
                TC_MOSI_PIN);

  // Both CS pins high (inactive)
  GPIO_InitSet(TC1_CS_PIN, MGPIO_MODE_OUT_PP, 0);
  GPIO_InitSet(TC2_CS_PIN, MGPIO_MODE_OUT_PP, 0);
  GPIO_SetLevel(TC1_CS_PIN, 1);
  GPIO_SetLevel(TC2_CS_PIN, 1);

  // Initialize per-sensor state
  for (int i = 0; i < TC_COUNT; i++)
  {
    sensors[i].lastReading.temperature = 0.0f;
    sensors[i].lastReading.status = TC_NOT_READY;
    sensors[i].lastReading.timestamp = 0;
    sensors[i].filterIndex = 0;
    sensors[i].filterFilled = false;

    for (int j = 0; j < TC_FILTER_SAMPLES; j++)
      sensors[i].filterBuffer[j] = 0.0f;
  }

  sensors[TC_BOARD].csPin = TC1_CS_PIN;
  sensors[TC_AMBIENT].csPin = TC2_CS_PIN;

  lastReadTime = 0;
  nextSensor = 0;
}

// Read raw 16 bits from a specific sensor's MAX6675
static uint16_t readRawSensor(TC_Sensor sensor)
{
  uint16_t csPin = sensors[sensor].csPin;

  // Pull CS low to begin read
  GPIO_SetLevel(csPin, 0);
  Delay_us(10);

  // Read 16 bits
  uint16_t raw = SW_SPI_Read_Write(&tc_spi, 0x0000);

  // Pull CS high — starts next conversion (~220ms)
  GPIO_SetLevel(csPin, 1);

  return raw;
}

TC_Reading MAX6675_Read(TC_Sensor sensor)
{
  TC_Reading reading;
  reading.timestamp = OS_GetTimeMs();

  uint16_t raw = readRawSensor(sensor);

  // Bit 2: thermocouple open flag (1 = open / not connected)
  if (raw & 0x04)
  {
    reading.temperature = 0.0f;
    reading.status = TC_OPEN;
    sensors[sensor].lastReading = reading;
    return reading;
  }

  // Bits 15-3: 12-bit temperature value, MSB first
  // Each bit = 0.25C
  float temp = (float)((raw >> 3) & 0x0FFF) * 0.25f;

  // Range validation
  if (temp < TC_MIN_VALID_TEMP || temp > TC_MAX_VALID_TEMP)
  {
    reading.temperature = temp;
    reading.status = TC_RANGE_ERROR;
    sensors[sensor].lastReading = reading;
    return reading;
  }

  // Update moving average filter
  TC_SensorState *s = &sensors[sensor];
  s->filterBuffer[s->filterIndex] = temp;
  s->filterIndex = (s->filterIndex + 1) % TC_FILTER_SAMPLES;
  if (s->filterIndex == 0)
    s->filterFilled = true;

  reading.temperature = temp;
  reading.status = TC_OK;
  s->lastReading = reading;

  return reading;
}

TC_Reading MAX6675_GetLastReading(TC_Sensor sensor)
{
  return sensors[sensor].lastReading;
}

static float getFilteredTempRaw(TC_Sensor sensor)
{
  TC_SensorState *s = &sensors[sensor];
  uint8_t count = s->filterFilled ? TC_FILTER_SAMPLES : s->filterIndex;
  if (count == 0)
    return s->lastReading.temperature;

  float sum = 0.0f;
  for (uint8_t i = 0; i < count; i++)
    sum += s->filterBuffer[i];

  return sum / (float)count;
}

float MAX6675_GetRawFilteredTemp(TC_Sensor sensor)
{
  return getFilteredTempRaw(sensor);
}

float MAX6675_GetFilteredTemp(TC_Sensor sensor)
{
  float raw = getFilteredTempRaw(sensor);

  // Apply calibration correction to board sensor only
  if (sensor == TC_BOARD && calData.enabled && calData.numPoints >= 2)
    return MAX6675_CalibrateTemp(raw);

  return raw;
}

bool MAX6675_IsConnected(TC_Sensor sensor)
{
  return (sensors[sensor].lastReading.status == TC_OK);
}

void MAX6675_Update(void)
{
  uint32_t now = OS_GetTimeMs();

  // Read one sensor per cycle, alternating between board and ambient
  // Each sensor needs ~250ms between reads, so at 250ms interval
  // alternating gives each sensor ~500ms between reads (plenty)
  if ((now - lastReadTime) >= TC_READ_INTERVAL_MS)
  {
    MAX6675_Read(nextSensor);
    nextSensor = (nextSensor + 1) % TC_COUNT;
    lastReadTime = now;
  }
}

// =============================================================================
// Calibration: piecewise linear correction
// =============================================================================

float MAX6675_CalibrateTemp(float rawC)
{
  if (calData.numPoints < 2)
    return rawC;

  // Below first calibration point: extrapolate from first segment
  if (rawC <= calData.points[0].sensorC)
  {
    float slope = (calData.points[1].actualC - calData.points[0].actualC) /
                  (calData.points[1].sensorC - calData.points[0].sensorC);
    return calData.points[0].actualC + slope * (rawC - calData.points[0].sensorC);
  }

  // Find the segment
  for (uint8_t i = 0; i < calData.numPoints - 1; i++)
  {
    if (rawC <= calData.points[i + 1].sensorC)
    {
      float dSensor = calData.points[i + 1].sensorC - calData.points[i].sensorC;
      if (dSensor < 0.1f) return calData.points[i].actualC;  // avoid divide by zero

      float t = (rawC - calData.points[i].sensorC) / dSensor;
      return calData.points[i].actualC + t * (calData.points[i + 1].actualC - calData.points[i].actualC);
    }
  }

  // Above last calibration point: extrapolate from last segment
  uint8_t n = calData.numPoints;
  float slope = (calData.points[n - 1].actualC - calData.points[n - 2].actualC) /
                (calData.points[n - 1].sensorC - calData.points[n - 2].sensorC);
  return calData.points[n - 1].actualC + slope * (rawC - calData.points[n - 1].sensorC);
}

void MAX6675_SetCalibration(const TC_Calibration *cal)
{
  memcpy(&calData, cal, sizeof(TC_Calibration));
}

const TC_Calibration * MAX6675_GetCalibration(void)
{
  return &calData;
}

// =============================================================================
// Calibration: SD card persistence (TCAL.DAT)
// =============================================================================

bool MAX6675_SaveCalibration(void)
{
  if (!mountSDCard())
    return false;

  FIL file;
  if (f_open(&file, "TCAL.DAT", FA_CREATE_ALWAYS | FA_WRITE) != FR_OK)
    return false;

  char buf[64];
  UINT bw;

  sprintf(buf, "TCAL\n%d\n", calData.numPoints);
  f_write(&file, buf, strlen(buf), &bw);

  for (uint8_t i = 0; i < calData.numPoints; i++)
  {
    sprintf(buf, "%.1f,%.1f\n",
            (double)calData.points[i].sensorC,
            (double)calData.points[i].actualC);
    f_write(&file, buf, strlen(buf), &bw);
  }

  f_close(&file);
  return true;
}

bool MAX6675_LoadCalibration(void)
{
  if (!mountSDCard())
    return false;

  FIL file;
  if (f_open(&file, "TCAL.DAT", FA_READ) != FR_OK)
    return false;

  // Read entire file (small — max ~200 bytes)
  char buf[256];
  UINT br;
  FRESULT res = f_read(&file, buf, sizeof(buf) - 1, &br);
  f_close(&file);

  if (res != FR_OK || br < 10)
    return false;

  buf[br] = '\0';

  // Parse: "TCAL\nN\nS1,A1\nS2,A2\n..."
  if (strncmp(buf, "TCAL", 4) != 0)
    return false;

  char *p = buf + 4;
  while (*p == '\n' || *p == '\r') p++;

  int numPoints = 0;
  sscanf(p, "%d", &numPoints);
  if (numPoints < 2 || numPoints > TC_CAL_POINTS)
    return false;

  // Skip past the number line
  while (*p && *p != '\n') p++;
  while (*p == '\n' || *p == '\r') p++;

  calData.numPoints = (uint8_t)numPoints;

  for (uint8_t i = 0; i < calData.numPoints; i++)
  {
    if (*p == '\0')
    {
      calData.numPoints = 0;
      calData.enabled = false;
      return false;
    }

    double s, a;
    if (sscanf(p, "%lf,%lf", &s, &a) != 2)
    {
      calData.numPoints = 0;
      calData.enabled = false;
      return false;
    }
    calData.points[i].sensorC = (float)s;
    calData.points[i].actualC = (float)a;

    // Skip to next line
    while (*p && *p != '\n') p++;
    while (*p == '\n' || *p == '\r') p++;
  }

  calData.enabled = true;
  return true;
}
