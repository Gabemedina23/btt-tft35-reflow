#include "includes.h"
#include "reflow_log.h"

// =============================================================================
// SD Card CSV Logger
//
// Writes temperature and control data to CSV files on the SD card.
// Files are named RFLOW_XX.CSV where XX auto-increments.
// =============================================================================

static FIL logFile;
static bool logActive = false;
static char logFilename[20];
static uint32_t logStartTime = 0;
static uint16_t logRowCount = 0;

// Find the next available log file number
static uint8_t findNextFileNumber(void)
{
  FILINFO fno;
  char fname[20];

  for (uint8_t i = 0; i < 100; i++)
  {
    sprintf(fname, "RFLOW_%02d.CSV", i);
    if (f_stat(fname, &fno) != FR_OK)  // file doesn't exist
      return i;
  }
  return 99;  // overwrite last if full
}

void ReflowLog_Init(void)
{
  logActive = false;
  logRowCount = 0;
}

bool ReflowLog_Start(const char *sessionName)
{
  if (logActive)
    ReflowLog_Stop();

  // Mount SD card
  if (!mountSDCard())
    return false;

  uint8_t num = findNextFileNumber();
  sprintf(logFilename, "RFLOW_%02d.CSV", num);

  FRESULT res = f_open(&logFile, logFilename, FA_CREATE_ALWAYS | FA_WRITE);
  if (res != FR_OK)
    return false;

  // Write CSV header
  char header[128];
  sprintf(header, "elapsed_s,board_c,ambient_c,target_c,duty_pct,state,stage,session:%s\n",
          sessionName);
  UINT bw;
  f_write(&logFile, header, strlen(header), &bw);
  f_sync(&logFile);

  logActive = true;
  logStartTime = OS_GetTimeMs();
  logRowCount = 0;

  return true;
}

void ReflowLog_Write(float boardTemp, float ambientTemp, float targetTemp,
                     float dutyCycle, const char *state, const char *stage)
{
  if (!logActive) return;

  uint32_t elapsed = (OS_GetTimeMs() - logStartTime) / 1000;

  char row[128];
  sprintf(row, "%lu,%.1f,%.1f,%.1f,%.1f,%s,%s\n",
          (unsigned long)elapsed,
          (double)boardTemp, (double)ambientTemp, (double)targetTemp,
          (double)dutyCycle, state, stage);

  UINT bw;
  f_write(&logFile, row, strlen(row), &bw);

  logRowCount++;

  // Sync to SD every 10 rows to avoid data loss without killing performance
  if ((logRowCount % 10) == 0)
    f_sync(&logFile);
}

void ReflowLog_Event(const char *event)
{
  if (!logActive) return;

  uint32_t elapsed = (OS_GetTimeMs() - logStartTime) / 1000;

  char row[128];
  sprintf(row, "%lu,,,,,EVENT,%s\n", (unsigned long)elapsed, event);

  UINT bw;
  f_write(&logFile, row, strlen(row), &bw);
  f_sync(&logFile);
}

void ReflowLog_Stop(void)
{
  if (!logActive) return;

  ReflowLog_Event("Session ended");
  f_sync(&logFile);
  f_close(&logFile);
  logActive = false;
}

bool ReflowLog_IsActive(void)
{
  return logActive;
}

const char * ReflowLog_GetFilename(void)
{
  return logFilename;
}
