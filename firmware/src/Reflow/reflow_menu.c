#include "includes.h"
#include "reflow_menu.h"
#include "reflow_control.h"
#include "reflow_profile.h"
#include "max6675.h"
#include "reflow_pins.h"
#include <math.h>

// =============================================================================
// Display layout constants (480x320 TFT)
// =============================================================================

// Graph area
#define GRAPH_X       45   // left margin (room for Y-axis labels)
#define GRAPH_Y       35   // top margin (room for header text)
#define GRAPH_W       420  // graph width
#define GRAPH_H       200  // graph height
#define GRAPH_X2      (GRAPH_X + GRAPH_W)
#define GRAPH_Y2      (GRAPH_Y + GRAPH_H)

// Status bar area (below graph)
#define STATUS_Y      (GRAPH_Y2 + 8)
#define STATUS_H      (LCD_HEIGHT - STATUS_Y)

// Colors for reflow stages
#define COLOR_PREHEAT   YELLOW
#define COLOR_SOAK      ORANGE
#define COLOR_RAMP      MAT_RED
#define COLOR_PEAK      RED
#define COLOR_COOL      CYAN
#define COLOR_TARGET    0x630C   // dark gray for target profile line
#define COLOR_GRAPH_BG  BLACK
#define COLOR_GRID      0x2104   // very dark gray grid lines
#define COLOR_AXIS      WHITE
#define COLOR_TEXT       WHITE
#define COLOR_ERROR     RED

// Refresh rate
#define GRAPH_REFRESH_MS  1000

// =============================================================================
// Helper: map temperature/time to pixel coordinates
// =============================================================================

static float graphTempMax = 280.0f;
static float graphTimeMax = 360.0f;  // 6 minutes default

static uint16_t tempToY(float temp)
{
  if (temp < 0) temp = 0;
  if (temp > graphTempMax) temp = graphTempMax;
  // Y is inverted (0 at top)
  return GRAPH_Y2 - (uint16_t)((temp / graphTempMax) * GRAPH_H);
}

static uint16_t timeToX(float timeSec)
{
  if (timeSec < 0) timeSec = 0;
  if (timeSec > graphTimeMax) timeSec = graphTimeMax;
  return GRAPH_X + (uint16_t)((timeSec / graphTimeMax) * GRAPH_W);
}

// =============================================================================
// Get color for current stage
// =============================================================================

static uint16_t getStageColor(ReflowState state)
{
  switch (state)
  {
    case REFLOW_PREHEAT:  return COLOR_PREHEAT;
    case REFLOW_SOAK:     return COLOR_SOAK;
    case REFLOW_RAMP:     return COLOR_RAMP;
    case REFLOW_PEAK:     return COLOR_PEAK;
    case REFLOW_COOL:     return COLOR_COOL;
    case REFLOW_ERROR:    return COLOR_ERROR;
    default:              return GREEN;
  }
}

// =============================================================================
// Draw graph axes and grid
// =============================================================================

static void drawGraphFrame(void)
{
  char str[8];

  // Background
  GUI_SetColor(COLOR_GRAPH_BG);
  GUI_FillRect(GRAPH_X, GRAPH_Y, GRAPH_X2, GRAPH_Y2);

  // Grid lines (horizontal - temperature)
  GUI_SetColor(COLOR_GRID);
  for (int t = 50; t <= (int)graphTempMax; t += 50)
  {
    uint16_t y = tempToY((float)t);
    GUI_HLine(GRAPH_X, y, GRAPH_X2);
  }

  // Grid lines (vertical - time)
  for (int s = 60; s <= (int)graphTimeMax; s += 60)
  {
    uint16_t x = timeToX((float)s);
    GUI_VLine(x, GRAPH_Y, GRAPH_Y2);
  }

  // Axes
  GUI_SetColor(COLOR_AXIS);
  GUI_HLine(GRAPH_X, GRAPH_Y2, GRAPH_X2);  // X axis
  GUI_VLine(GRAPH_X, GRAPH_Y, GRAPH_Y2);   // Y axis

  // Y-axis labels (temperature)
  GUI_SetColor(COLOR_TEXT);
  for (int t = 50; t <= (int)graphTempMax; t += 50)
  {
    uint16_t y = tempToY((float)t);
    sprintf(str, "%d", t);
    _GUI_DispStringRight(GRAPH_X - 3, y - BYTE_HEIGHT / 2, (uint8_t *)str);
  }

  // X-axis labels (time in minutes)
  for (int s = 60; s <= (int)graphTimeMax; s += 60)
  {
    uint16_t x = timeToX((float)s);
    sprintf(str, "%dm", s / 60);
    _GUI_DispStringCenter(x, GRAPH_Y2 + 2, (uint8_t *)str);
  }
}

// =============================================================================
// Draw the target profile curve (dashed gray)
// =============================================================================

static void drawTargetProfile(const ReflowProfile *profile, float startTemp)
{
  GUI_SetColor(COLOR_TARGET);

  float curTemp = startTemp;
  float curTime = 0;
  uint16_t prevX = timeToX(0);
  uint16_t prevY = tempToY(startTemp);

  for (uint8_t i = 0; i < profile->numStages; i++)
  {
    const ReflowStage *stage = &profile->stages[i];
    float tempDelta = fabsf(stage->targetTemp - curTemp);
    float rampRate = fabsf(stage->rampRate);
    float rampTime = (rampRate > 0.01f) ? (tempDelta / rampRate) : 0;

    // End of ramp
    float rampEndTime = curTime + rampTime;
    uint16_t x1 = timeToX(rampEndTime);
    uint16_t y1 = tempToY(stage->targetTemp);

    // Draw ramp line (dashed: draw every other 4px segment)
    // For simplicity, draw solid thin line in gray
    GUI_DrawLine(prevX, prevY, x1, y1);

    // Hold period
    if (stage->holdTime > 0)
    {
      float holdEndTime = rampEndTime + stage->holdTime;
      uint16_t x2 = timeToX(holdEndTime);
      GUI_HLine(x1, y1, x2);
      curTime = holdEndTime;
      prevX = x2;
    }
    else
    {
      curTime = rampEndTime;
      prevX = x1;
    }

    prevY = y1;
    curTemp = stage->targetTemp;
  }
}

// =============================================================================
// Draw actual temperature history
// =============================================================================

static void drawTempHistory(const ReflowController *state)
{
  const TempHistory *hist = &state->history;
  if (hist->count < 2) return;

  uint16_t color = getStageColor(state->state);
  GUI_SetColor(color);

  uint16_t start = 0;
  if (hist->count >= TEMP_HISTORY_SIZE)
    start = hist->head;

  uint16_t prevIdx = start;
  float prevTime = 0;
  uint16_t prevX = timeToX(0);
  uint16_t prevY = tempToY(hist->samples[prevIdx]);

  for (uint16_t i = 1; i < hist->count; i++)
  {
    uint16_t idx = (start + i) % TEMP_HISTORY_SIZE;
    float t = (float)i;  // 1 sample per second
    uint16_t x = timeToX(t);
    uint16_t y = tempToY(hist->samples[idx]);

    // Clip to graph area
    if (x > GRAPH_X && x < GRAPH_X2 && prevX >= GRAPH_X)
    {
      GUI_DrawLine(prevX, prevY, x, y);
    }

    prevX = x;
    prevY = y;
    (void)prevTime;
    prevTime = t;
  }

  // Draw current position marker (filled circle)
  if (hist->count > 0)
  {
    uint16_t lastIdx = (start + hist->count - 1) % TEMP_HISTORY_SIZE;
    uint16_t cx = timeToX((float)(hist->count - 1));
    uint16_t cy = tempToY(hist->samples[lastIdx]);
    if (cx > GRAPH_X + 3 && cx < GRAPH_X2 - 3)
    {
      GUI_SetColor(WHITE);
      GUI_FillCircle(cx, cy, 3);
      GUI_SetColor(color);
      GUI_DrawCircle(cx, cy, 3);
    }
  }
}

// =============================================================================
// Draw status bar (below graph)
// =============================================================================

static void drawStatusBar(const ReflowController *state)
{
  char str[64];
  uint16_t y = STATUS_Y;

  // Clear status area
  GUI_SetColor(infoSettings.bg_color);
  GUI_FillRect(0, y, LCD_WIDTH, LCD_HEIGHT);

  // Left side: Current temp, target, duty
  GUI_SetColor(getStageColor(state->state));
  sprintf(str, "%.1fC", (double)state->currentTemp);
  _GUI_DispString(5, y, (uint8_t *)str);

  GUI_SetColor(COLOR_TEXT);
  sprintf(str, "/ %.0fC", (double)state->targetTemp);
  _GUI_DispString(100, y, (uint8_t *)str);

  sprintf(str, "Duty:%.0f%%", (double)state->dutyCycle);
  _GUI_DispString(210, y, (uint8_t *)str);

  // Stage name
  GUI_SetColor(getStageColor(state->state));
  sprintf(str, "%s", Reflow_GetStateName(state->state));
  _GUI_DispStringRight(LCD_WIDTH - 5, y, (uint8_t *)str);

  // Second row: elapsed / remaining, profile name
  y += BYTE_HEIGHT + 2;
  GUI_SetColor(COLOR_TEXT);

  uint32_t elapsed = Reflow_GetElapsedTime();
  uint32_t remaining = Reflow_GetRemainingTime();
  sprintf(str, "Time: %lu:%02lu / ~%lu:%02lu",
          (unsigned long)(elapsed / 60), (unsigned long)(elapsed % 60),
          (unsigned long)((elapsed + remaining) / 60), (unsigned long)((elapsed + remaining) % 60));
  _GUI_DispString(5, y, (uint8_t *)str);

  // Ambient temperature
  GUI_SetColor(CYAN);
  sprintf(str, "Amb:%.1fC", (double)state->ambientTemp);
  _GUI_DispStringRight(LCD_WIDTH - 5, y, (uint8_t *)str);
}

// =============================================================================
// Draw header bar
// =============================================================================

static void drawHeader(const char *profileName, const ReflowController *state)
{
  char str[64];

  GUI_SetColor(infoSettings.bg_color);
  GUI_FillRect(0, 0, LCD_WIDTH, GRAPH_Y - 2);

  GUI_SetColor(COLOR_TEXT);
  sprintf(str, "Profile: %s", profileName);
  _GUI_DispString(5, 5, (uint8_t *)str);

  // Encoder button hint
  GUI_SetColor(MAT_RED);
  _GUI_DispStringRight(LCD_WIDTH - 5, 5, (uint8_t *)"[Click=ABORT]");
}

// =============================================================================
// Main Reflow Menu (entry point)
// =============================================================================

void menuReflowMain(void)
{
  MENUITEMS reflowItems = {
    // title
    LABEL_CUSTOM,  // reuse existing label slot for title
    // icon                          label
    {
      {ICON_HEAT_FAN,                {.address = (void *)"Start"}},
      {ICON_CUSTOM,                  {.address = (void *)"Profiles"}},
      {ICON_FEATURE_SETTINGS,        {.address = (void *)"Auto Tune"}},
      {ICON_SCREEN_INFO,             {.address = (void *)"Monitor"}},
      {ICON_SETTINGS,                {.address = (void *)"Settings"}},
      {ICON_HEAT_FAN,                {.address = (void *)"Burn-In"}},
      {ICON_NULL,                    {.index = LABEL_NULL}},
      {ICON_NULL,                    {.index = LABEL_NULL}},
    }
  };

  KEY_VALUES key_num = KEY_IDLE;

  menuDrawPage(&reflowItems);

  while (MENU_IS(menuReflowMain))
  {
    key_num = menuKeyGetValue();

    switch (key_num)
    {
      case KEY_ICON_0:  // Start Reflow
      {
        // Start with default leaded profile (TODO: remember last selection)
        Reflow_Start(Profile_GetLeaded());
        OPEN_MENU(menuReflowActive);
        break;
      }

      case KEY_ICON_1:  // Profiles
        OPEN_MENU(menuReflowProfiles);
        break;

      case KEY_ICON_2:  // Auto Tune
        OPEN_MENU(menuReflowPIDTune);
        break;

      case KEY_ICON_3:  // Monitor
        OPEN_MENU(menuReflowMonitor);
        break;

      case KEY_ICON_4:  // Settings
        OPEN_MENU(menuReflowSettings);
        break;

      case KEY_ICON_5:  // Burn-In
        OPEN_MENU(menuReflowBurnIn);
        break;

      default:
        break;
    }

    loopProcess();
  }
}

// =============================================================================
// Active Reflow Screen (live during reflow)
// =============================================================================

void menuReflowActive(void)
{
  const GUI_RECT fullRect = {0, 0, LCD_WIDTH - 1, LCD_HEIGHT - 1};
  const ReflowController *state = Reflow_GetState();
  bool needsFullRedraw = true;

  // Calculate graph scaling from profile
  graphTempMax = Profile_GetPeakTemp(&state->profile) + 50.0f;
  if (graphTempMax < 280.0f) graphTempMax = 280.0f;
  graphTimeMax = (float)Profile_GetExpectedDuration(&state->profile, 25.0f) + 60.0f;
  if (graphTimeMax < 300.0f) graphTimeMax = 300.0f;

  setMenu(MENU_TYPE_FULLSCREEN, NULL, 1, &fullRect, NULL, NULL);

  while (MENU_IS(menuReflowActive))
  {
    // Run the reflow control loop
    Reflow_Update();

    // Check for abort (encoder click or touch)
    if (LCD_Enc_ReadBtn(200))
    {
      Reflow_Abort();
    }

    if (KEY_GetValue(1, &fullRect) == 0)
    {
      // Touch detected — abort if running, exit if done
      if (state->state >= REFLOW_COMPLETE)
      {
        Reflow_Reset();
        CLOSE_MENU();
        break;
      }
      else if (state->state != REFLOW_IDLE)
      {
        Reflow_Abort();
      }
    }

    // Refresh display at 1Hz
    if (nextScreenUpdate(GRAPH_REFRESH_MS) || needsFullRedraw)
    {
      if (needsFullRedraw)
      {
        GUI_Clear(infoSettings.bg_color);
        drawGraphFrame();
        drawTargetProfile(&state->profile, 25.0f);
        drawHeader(state->profile.name, state);
        needsFullRedraw = false;
      }

      // Redraw temperature trace and status (incremental)
      // Clear only the graph interior for redraw
      GUI_SetColor(COLOR_GRAPH_BG);
      GUI_FillRect(GRAPH_X + 1, GRAPH_Y + 1, GRAPH_X2 - 1, GRAPH_Y2 - 1);

      // Redraw grid
      GUI_SetColor(COLOR_GRID);
      for (int t = 50; t <= (int)graphTempMax; t += 50)
        GUI_HLine(GRAPH_X + 1, tempToY((float)t), GRAPH_X2 - 1);
      for (int s = 60; s <= (int)graphTimeMax; s += 60)
        GUI_VLine(timeToX((float)s), GRAPH_Y + 1, GRAPH_Y2 - 1);

      // Target profile
      drawTargetProfile(&state->profile, 25.0f);

      // Actual temperature trace
      drawTempHistory(state);

      // Status bar
      drawStatusBar(state);

      // Flashing "OPEN DOOR" during cooldown
      if (state->state == REFLOW_COOL)
      {
        static bool flashToggle = false;
        flashToggle = !flashToggle;
        if (flashToggle)
        {
          GUI_SetColor(CYAN);
          _GUI_DispStringInRect(GRAPH_X, GRAPH_Y + 10,
                                GRAPH_X2, GRAPH_Y + 10 + BYTE_HEIGHT * 2,
                                (uint8_t *)">>> OPEN DOOR <<<");
        }
      }
    }

    // Check if reflow finished or errored
    if (state->state == REFLOW_COMPLETE)
    {
      // Show completion message
      Buzzer_Play(SOUND_SUCCESS);

      GUI_SetColor(GREEN);
      _GUI_DispStringInRect(0, GRAPH_Y + GRAPH_H / 2 - BYTE_HEIGHT,
                            LCD_WIDTH, GRAPH_Y + GRAPH_H / 2 + BYTE_HEIGHT,
                            (uint8_t *)"REFLOW COMPLETE - Touch to exit");
    }
    else if (state->state == REFLOW_ERROR)
    {
      Buzzer_Play(SOUND_ERROR);

      GUI_SetColor(RED);
      char errStr[64];
      sprintf(errStr, "ERROR: %s - Touch to exit", Reflow_GetErrorName(state->error));
      _GUI_DispStringInRect(0, GRAPH_Y + GRAPH_H / 2 - BYTE_HEIGHT,
                            LCD_WIDTH, GRAPH_Y + GRAPH_H / 2 + BYTE_HEIGHT,
                            (uint8_t *)errStr);
    }
    else if (state->state == REFLOW_ABORTED)
    {
      Buzzer_Play(SOUND_OK);  // short clean beep, not the harsh cancel sound

      GUI_SetColor(YELLOW);
      _GUI_DispStringInRect(0, GRAPH_Y + GRAPH_H / 2 - BYTE_HEIGHT,
                            LCD_WIDTH, GRAPH_Y + GRAPH_H / 2 + BYTE_HEIGHT,
                            (uint8_t *)"ABORTED - Touch to exit");
    }

    loopProcess();
  }
}

// =============================================================================
// Profile Selection Menu
// =============================================================================

void menuReflowProfiles(void)
{
  MENUITEMS profileItems = {
    LABEL_CUSTOM,  // reuse existing label slot for title
    {
      {ICON_HEAT_FAN,     {.address = (void *)"Leaded"}},
      {ICON_HEAT_FAN,     {.address = (void *)"Lead-Free"}},
      {ICON_NULL,         {.index = LABEL_NULL}},
      {ICON_NULL,         {.index = LABEL_NULL}},
      {ICON_NULL,         {.index = LABEL_NULL}},
      {ICON_NULL,         {.index = LABEL_NULL}},
      {ICON_NULL,         {.index = LABEL_NULL}},
      {ICON_BACK,         {.index = LABEL_BACK}},
    }
  };

  KEY_VALUES key_num = KEY_IDLE;
  menuDrawPage(&profileItems);

  while (MENU_IS(menuReflowProfiles))
  {
    key_num = menuKeyGetValue();

    switch (key_num)
    {
      case KEY_ICON_0:  // Leaded
        Reflow_Start(Profile_GetLeaded());
        OPEN_MENU(menuReflowActive);
        break;

      case KEY_ICON_1:  // Lead-Free
        Reflow_Start(Profile_GetLeadFree());
        OPEN_MENU(menuReflowActive);
        break;

      case KEY_ICON_7:  // Back
        CLOSE_MENU();
        break;

      default:
        break;
    }

    loopProcess();
  }
}

// =============================================================================
// Profile Editor (placeholder)
// =============================================================================

void menuReflowProfileEdit(void)
{
  // TODO: Implement stage-by-stage editing with numpad
  CLOSE_MENU();
}

// =============================================================================
// PID Auto-Tune Menu
// =============================================================================

void menuReflowPIDTune(void)
{
  const GUI_RECT fullRect = {0, 0, LCD_WIDTH - 1, LCD_HEIGHT - 1};
  char str[64];
  bool needsRedraw = true;

  AutotuneContext atCtx;
  Autotune_Init(&atCtx, 150.0f);  // Tune around 150°C (safe for oven)
  Autotune_Start(&atCtx);

  ReflowLog_Start("autotune");
  ReflowLog_Event("PID auto-tune started at 150C");

  // SSR already initialized by Reflow_Init() at boot

  GUI_Clear(infoSettings.bg_color);
  setMenu(MENU_TYPE_FULLSCREEN, NULL, 1, &fullRect, NULL, NULL);

  while (MENU_IS(menuReflowPIDTune))
  {
    // Update thermocouples
    MAX6675_Update();
    float boardTemp = MAX6675_GetFilteredTemp(TC_BOARD);
    float ambientTemp = MAX6675_GetFilteredTemp(TC_AMBIENT);

    // Run auto-tune relay logic
    float duty = Autotune_Update(&atCtx, boardTemp);

    // Apply SSR PWM
    static uint32_t ssrStart = 0;
    uint32_t now = OS_GetTimeMs();
    if ((now - ssrStart) >= PID_SSR_PERIOD_MS) ssrStart = now;
    uint32_t onTime = (uint32_t)(duty * PID_SSR_PERIOD_MS / 100.0f);
    bool ssrOn = ((now - ssrStart) < onTime);
    GPIO_SetLevel(SSR_PIN, SSR_ACTIVE_LOW ? (ssrOn ? 0 : 1) : (ssrOn ? 1 : 0));

    // Log data
    static uint32_t lastLogTime = 0;
    if ((now - lastLogTime) >= 1000)
    {
      const char *stateStr =
        atCtx.state == AUTOTUNE_HEATING ? "Heating" :
        atCtx.state == AUTOTUNE_RELAY_ON ? "RelayON" :
        atCtx.state == AUTOTUNE_RELAY_OFF ? "RelayOFF" : "Done";
      sprintf(str, "cycle%d", atCtx.cycleCount);
      ReflowLog_Write(boardTemp, ambientTemp, atCtx.targetTemp, duty, stateStr, str);
      lastLogTime = now;
    }

    // Check for abort (touch)
    if (KEY_GetValue(1, &fullRect) == 0)
    {
      if (!Autotune_IsRunning(&atCtx))
      {
        // Done or error — exit
        GPIO_SetLevel(SSR_PIN, SSR_ACTIVE_LOW ? 1 : 0);  // SSR off
        ReflowLog_Stop();
        CLOSE_MENU();
        break;
      }
      else
      {
        // Abort
        Autotune_Abort(&atCtx);
        GPIO_SetLevel(SSR_PIN, SSR_ACTIVE_LOW ? 1 : 0);
        ReflowLog_Event("Auto-tune aborted");
        ReflowLog_Stop();
        Buzzer_Play(SOUND_OK);
      }
    }

    // Encoder click = abort
    if (LCD_Enc_ReadBtn(200) && Autotune_IsRunning(&atCtx))
    {
      Autotune_Abort(&atCtx);
      GPIO_SetLevel(SSR_PIN, SSR_ACTIVE_LOW ? 1 : 0);
      ReflowLog_Event("Auto-tune aborted");
      ReflowLog_Stop();
      Buzzer_Play(SOUND_OK);
    }

    // Refresh display
    if (nextScreenUpdate(500) || needsRedraw)
    {
      needsRedraw = false;

      GUI_SetColor(infoSettings.bg_color);
      GUI_FillRect(0, 0, LCD_WIDTH, LCD_HEIGHT);

      GUI_SetColor(COLOR_TEXT);
      _GUI_DispString(5, 5, (uint8_t *)"PID Auto-Tune");
      GUI_HLine(0, 28, LCD_WIDTH);

      uint16_t y = 35;

      // Status
      GUI_SetColor(YELLOW);
      const char *stateStr =
        atCtx.state == AUTOTUNE_HEATING ? "Heating to target..." :
        atCtx.state == AUTOTUNE_RELAY_ON ? "Relay ON (heating)" :
        atCtx.state == AUTOTUNE_RELAY_OFF ? "Relay OFF (cooling)" :
        atCtx.state == AUTOTUNE_COMPLETE ? "COMPLETE!" :
        atCtx.state == AUTOTUNE_ERROR ? "TIMEOUT ERROR" : "Idle";
      _GUI_DispString(10, y, (uint8_t *)stateStr);

      // Temperatures
      y += BYTE_HEIGHT + 8;
      GUI_SetColor(GREEN);
      sprintf(str, "Board: %.1f C    Target: %.0f C", (double)boardTemp, (double)atCtx.targetTemp);
      _GUI_DispString(10, y, (uint8_t *)str);

      y += BYTE_HEIGHT + 4;
      GUI_SetColor(CYAN);
      sprintf(str, "Ambient: %.1f C  Duty: %.0f%%", (double)ambientTemp, (double)duty);
      _GUI_DispString(10, y, (uint8_t *)str);

      // Cycle info
      y += BYTE_HEIGHT + 8;
      GUI_SetColor(COLOR_TEXT);
      sprintf(str, "Cycles: %d / %d    Peaks: %d  Valleys: %d",
              atCtx.cycleCount, atCtx.minCycles, atCtx.peakCount, atCtx.valleyCount);
      _GUI_DispString(10, y, (uint8_t *)str);

      // Results (if complete)
      if (atCtx.state == AUTOTUNE_COMPLETE)
      {
        y += BYTE_HEIGHT + 8;
        GUI_SetColor(GREEN);
        _GUI_DispString(10, y, (uint8_t *)"Results:");

        y += BYTE_HEIGHT + 4;
        sprintf(str, "  Ku=%.1f  Tu=%.1fs", (double)atCtx.Ku, (double)atCtx.Tu);
        _GUI_DispString(10, y, (uint8_t *)str);

        y += BYTE_HEIGHT + 4;
        sprintf(str, "  Kp=%.1f  Ki=%.2f  Kd=%.1f",
                (double)atCtx.resultKp, (double)atCtx.resultKi, (double)atCtx.resultKd);
        _GUI_DispString(10, y, (uint8_t *)str);

        y += BYTE_HEIGHT + 8;
        GUI_SetColor(YELLOW);
        _GUI_DispString(10, y, (uint8_t *)"Touch to exit. Update profiles with these values.");

        char logMsg[64];
        sprintf(logMsg, "Kp=%.1f Ki=%.2f Kd=%.1f", (double)atCtx.resultKp,
                (double)atCtx.resultKi, (double)atCtx.resultKd);
        ReflowLog_Event(logMsg);
        ReflowLog_Stop();

        Buzzer_Play(SOUND_SUCCESS);
      }
      else if (atCtx.state == AUTOTUNE_ERROR)
      {
        y += BYTE_HEIGHT + 8;
        GUI_SetColor(RED);
        _GUI_DispString(10, y, (uint8_t *)"Auto-tune timed out. Touch to exit.");
      }
      else
      {
        // Running — show touch to abort
        GUI_SetColor(COLOR_TEXT);
        GUI_HLine(0, LCD_HEIGHT - (BYTE_HEIGHT * 2), LCD_WIDTH);
        _GUI_DispStringInRect(0, LCD_HEIGHT - (BYTE_HEIGHT * 2), LCD_WIDTH, LCD_HEIGHT,
                              (uint8_t *)"Touch or click encoder to abort");
      }
    }

    loopProcess();
  }
}

// =============================================================================
// Burn-In Mode (off-gas oven at 150°C)
// =============================================================================

void menuReflowBurnIn(void)
{
  const GUI_RECT fullRect = {0, 0, LCD_WIDTH - 1, LCD_HEIGHT - 1};
  char str[64];
  bool running = true;
  float targetTemp = 150.0f;
  float dutyCycle = 0.0f;
  PID_Controller burnPid;
  bool needsRedraw = true;

  PID_Init(&burnPid, 3.1f, 0.06f, 42.0f, PID_OUTPUT_MIN, PID_OUTPUT_MAX);

  ReflowLog_Start("burnin");
  char logMsg[48];
  sprintf(logMsg, "Burn-in started at %.0fC", (double)targetTemp);
  ReflowLog_Event(logMsg);

  // SSR already initialized by Reflow_Init() at boot

  GUI_Clear(infoSettings.bg_color);
  setMenu(MENU_TYPE_FULLSCREEN, NULL, 1, &fullRect, NULL, NULL);

  uint32_t startTime = OS_GetTimeMs();
  uint32_t lastPidTime = OS_GetTimeMs();

  while (MENU_IS(menuReflowBurnIn))
  {
    MAX6675_Update();
    float boardTemp = MAX6675_GetFilteredTemp(TC_BOARD);
    float ambientTemp = MAX6675_GetFilteredTemp(TC_AMBIENT);
    uint32_t now = OS_GetTimeMs();

    // PID control at 2Hz
    if (running && (now - lastPidTime) >= PID_UPDATE_INTERVAL_MS)
    {
      float dt = (float)(now - lastPidTime) / 1000.0f;
      lastPidTime = now;

      if (boardTemp < targetTemp - 10.0f)
        dutyCycle = PID_OUTPUT_MAX;
      else
        dutyCycle = PID_Compute(&burnPid, boardTemp, targetTemp, dt);

      // Log every second (alternating with PID at 2Hz)
      static bool logToggle = false;
      logToggle = !logToggle;
      if (logToggle)
        ReflowLog_Write(boardTemp, ambientTemp, targetTemp, dutyCycle, "BurnIn", "Hold");
    }

    // SSR PWM
    if (running)
    {
      static uint32_t ssrStart = 0;
      if ((now - ssrStart) >= PID_SSR_PERIOD_MS) ssrStart = now;
      uint32_t onTime = (uint32_t)(dutyCycle * PID_SSR_PERIOD_MS / 100.0f);
      bool ssrOn = ((now - ssrStart) < onTime);
      GPIO_SetLevel(SSR_PIN, SSR_ACTIVE_LOW ? (ssrOn ? 0 : 1) : (ssrOn ? 1 : 0));
    }
    else
    {
      GPIO_SetLevel(SSR_PIN, SSR_ACTIVE_LOW ? 1 : 0);
    }

    // Check for stop (touch or encoder)
    if (KEY_GetValue(1, &fullRect) == 0 || LCD_Enc_ReadBtn(200))
    {
      if (running)
      {
        running = false;
        dutyCycle = 0.0f;
        GPIO_SetLevel(SSR_PIN, SSR_ACTIVE_LOW ? 1 : 0);
        ReflowLog_Event("Burn-in stopped by user");
        ReflowLog_Stop();
        Buzzer_Play(SOUND_OK);
      }
      else
      {
        CLOSE_MENU();
        break;
      }
    }

    // Refresh display
    if (nextScreenUpdate(500) || needsRedraw)
    {
      needsRedraw = false;
      uint32_t elapsed = (now - startTime) / 1000;

      GUI_SetColor(infoSettings.bg_color);
      GUI_FillRect(0, 0, LCD_WIDTH, LCD_HEIGHT);

      GUI_SetColor(COLOR_TEXT);
      _GUI_DispString(5, 5, (uint8_t *)"Oven Burn-In (Off-Gas Mode)");
      GUI_HLine(0, 28, LCD_WIDTH);

      uint16_t y = 40;

      // Large temp display
      GUI_SetColor(running ? GREEN : YELLOW);
      sprintf(str, "Board: %.1f C", (double)boardTemp);
      _GUI_DispString(10, y, (uint8_t *)str);
      sprintf(str, "/ %.0f C", (double)targetTemp);
      _GUI_DispString(220, y, (uint8_t *)str);

      y += BYTE_HEIGHT + 8;
      GUI_SetColor(CYAN);
      sprintf(str, "Ambient: %.1f C", (double)ambientTemp);
      _GUI_DispString(10, y, (uint8_t *)str);

      y += BYTE_HEIGHT + 4;
      GUI_SetColor(COLOR_TEXT);
      sprintf(str, "Duty: %.0f%%    SSR: %s", (double)dutyCycle, running ? "ACTIVE" : "OFF");
      _GUI_DispString(10, y, (uint8_t *)str);

      y += BYTE_HEIGHT + 4;
      sprintf(str, "Elapsed: %lu:%02lu:%02lu",
              (unsigned long)(elapsed / 3600),
              (unsigned long)((elapsed % 3600) / 60),
              (unsigned long)(elapsed % 60));
      _GUI_DispString(10, y, (uint8_t *)str);

      y += BYTE_HEIGHT + 8;
      if (running)
      {
        GUI_SetColor(YELLOW);
        _GUI_DispString(10, y, (uint8_t *)"Let oven run until smoke stops.");
        y += BYTE_HEIGHT + 4;
        _GUI_DispString(10, y, (uint8_t *)"Open a window or do this in garage!");
      }
      else
      {
        GUI_SetColor(GREEN);
        _GUI_DispString(10, y, (uint8_t *)"Burn-in stopped. Oven cooling.");
        y += BYTE_HEIGHT + 4;
        _GUI_DispString(10, y, (uint8_t *)"Touch again to exit.");
      }

      // Logging indicator
      y += BYTE_HEIGHT + 8;
      if (ReflowLog_IsActive())
      {
        GUI_SetColor(GREEN);
        sprintf(str, "Logging to: %s", ReflowLog_GetFilename());
        _GUI_DispString(10, y, (uint8_t *)str);
      }

      // Bottom bar
      GUI_SetColor(COLOR_TEXT);
      GUI_HLine(0, LCD_HEIGHT - (BYTE_HEIGHT * 2), LCD_WIDTH);
      _GUI_DispStringInRect(0, LCD_HEIGHT - (BYTE_HEIGHT * 2), LCD_WIDTH, LCD_HEIGHT,
                            (uint8_t *)(running ? "Touch to stop burn-in" : "Touch to exit"));
    }

    loopProcess();
  }

  // Make sure SSR is off when leaving
  GPIO_SetLevel(SSR_PIN, SSR_ACTIVE_LOW ? 1 : 0);
  if (ReflowLog_IsActive()) ReflowLog_Stop();
}

// =============================================================================
// Temperature Monitor (passive, no heating)
// =============================================================================

void menuReflowMonitor(void)
{
  const GUI_RECT fullRect = {0, 0, LCD_WIDTH - 1, LCD_HEIGHT - 1};
  char str[48];
  bool firstDraw = true;

  GUI_Clear(infoSettings.bg_color);
  setMenu(MENU_TYPE_OTHER, NULL, 1, &fullRect, NULL, NULL);

  while (MENU_IS(menuReflowMonitor))
  {
    Reflow_Update();

    if (KEY_GetValue(1, &fullRect) == 0)
    {
      CLOSE_MENU();
      break;
    }

    if (nextScreenUpdate(500) || firstDraw)
    {
      firstDraw = false;
      TC_Reading boardReading = MAX6675_GetLastReading(TC_BOARD);
      TC_Reading ambientReading = MAX6675_GetLastReading(TC_AMBIENT);
      float boardTemp = MAX6675_GetFilteredTemp(TC_BOARD);
      float ambientTemp = MAX6675_GetFilteredTemp(TC_AMBIENT);

      // Clear display area
      GUI_SetColor(infoSettings.bg_color);
      GUI_FillRect(0, 0, LCD_WIDTH, LCD_HEIGHT - BYTE_HEIGHT * 2);

      // Title
      GUI_SetColor(COLOR_TEXT);
      _GUI_DispString(5, 5, (uint8_t *)"Temperature Monitor");
      GUI_HLine(0, 28, LCD_WIDTH);

      // === Board Thermocouple (TC1) ===
      uint16_t y = 38;
      GUI_SetColor(YELLOW);
      _GUI_DispString(10, y, (uint8_t *)"BOARD (TC1) - PID control");

      y += BYTE_HEIGHT + 4;
      if (boardReading.status == TC_OK)
      {
        GUI_SetColor(GREEN);
        sprintf(str, "  %.1f C", (double)boardTemp);
      }
      else if (boardReading.status == TC_OPEN)
      {
        GUI_SetColor(RED);
        sprintf(str, "  OPEN - Not connected");
      }
      else
      {
        GUI_SetColor(YELLOW);
        sprintf(str, "  Reading...");
      }
      _GUI_DispString(10, y, (uint8_t *)str);

      y += BYTE_HEIGHT + 4;
      GUI_SetColor(COLOR_TEXT);
      sprintf(str, "  Status: %s  Raw: %.2f C",
              boardReading.status == TC_OK ? "OK" :
              boardReading.status == TC_OPEN ? "OPEN" : "ERR",
              (double)boardReading.temperature);
      _GUI_DispString(10, y, (uint8_t *)str);

      // Divider
      y += BYTE_HEIGHT + 8;
      GUI_SetColor(COLOR_GRID);
      GUI_HLine(10, y, LCD_WIDTH - 10);
      y += 8;

      // === Ambient Thermocouple (TC2) ===
      GUI_SetColor(CYAN);
      _GUI_DispString(10, y, (uint8_t *)"AMBIENT (TC2) - Chamber temp");

      y += BYTE_HEIGHT + 4;
      if (ambientReading.status == TC_OK)
      {
        GUI_SetColor(GREEN);
        sprintf(str, "  %.1f C", (double)ambientTemp);
      }
      else if (ambientReading.status == TC_OPEN)
      {
        GUI_SetColor(RED);
        sprintf(str, "  OPEN - Not connected");
      }
      else
      {
        GUI_SetColor(YELLOW);
        sprintf(str, "  Reading...");
      }
      _GUI_DispString(10, y, (uint8_t *)str);

      y += BYTE_HEIGHT + 4;
      GUI_SetColor(COLOR_TEXT);
      sprintf(str, "  Status: %s  Raw: %.2f C",
              ambientReading.status == TC_OK ? "OK" :
              ambientReading.status == TC_OPEN ? "OPEN" : "ERR",
              (double)ambientReading.temperature);
      _GUI_DispString(10, y, (uint8_t *)str);

      // Bottom: touch to exit
      GUI_SetColor(COLOR_TEXT);
      GUI_HLine(0, LCD_HEIGHT - (BYTE_HEIGHT * 2), LCD_WIDTH);
      _GUI_DispStringInRect(0, LCD_HEIGHT - (BYTE_HEIGHT * 2), LCD_WIDTH, LCD_HEIGHT,
                            (uint8_t *)"Touch to go back");
    }

    loopProcess();
  }
}

// =============================================================================
// Settings Menu (placeholder)
// =============================================================================

void menuReflowSettings(void)
{
  // Two touch zones: top half = info (touch to exit), bottom = SSR test button
  const GUI_RECT exitRect = {0, 0, LCD_WIDTH - 1, LCD_HEIGHT / 2};
  const GUI_RECT ssrRect = {10, LCD_HEIGHT - BYTE_HEIGHT * 3 - 10,
                            LCD_WIDTH - 10, LCD_HEIGHT - 10};
  bool ssrTestActive = false;
  uint32_t ssrTestEnd = 0;
  bool needsRedraw = true;

  GUI_Clear(infoSettings.bg_color);
  setMenu(MENU_TYPE_FULLSCREEN, NULL, 1, &exitRect, NULL, NULL);

  while (MENU_IS(menuReflowSettings))
  {
    MAX6675_Update();
    uint32_t now = OS_GetTimeMs();

    // SSR test timeout (3 seconds)
    if (ssrTestActive && now >= ssrTestEnd)
    {
      ssrTestActive = false;
      GPIO_SetLevel(SSR_PIN, SSR_ACTIVE_LOW ? 1 : 0);  // SSR off
      needsRedraw = true;
    }

    // SSR test — keep SSR on while active
    if (ssrTestActive)
    {
      GPIO_SetLevel(SSR_PIN, SSR_ACTIVE_LOW ? 0 : 1);  // SSR on
    }

    // Check for touch
    if (KEY_GetValue(1, &ssrRect) == 0 && !ssrTestActive)
    {
      // SSR test button pressed
      ssrTestActive = true;
      ssrTestEnd = now + 3000;  // 3 seconds
      Buzzer_Play(SOUND_OK);
      needsRedraw = true;
    }
    else if (KEY_GetValue(1, &exitRect) == 0 && !ssrTestActive)
    {
      GPIO_SetLevel(SSR_PIN, SSR_ACTIVE_LOW ? 1 : 0);
      CLOSE_MENU();
      break;
    }

    if (nextScreenUpdate(500) || needsRedraw)
    {
      needsRedraw = false;
      char str[48];

      GUI_SetColor(infoSettings.bg_color);
      GUI_FillRect(0, 0, LCD_WIDTH, LCD_HEIGHT);

      GUI_SetColor(COLOR_TEXT);
      _GUI_DispString(5, 5, (uint8_t *)"Settings & Diagnostics");
      GUI_HLine(0, 28, LCD_WIDTH);

      uint16_t y = 35;

      // PID values
      GUI_SetColor(YELLOW);
      _GUI_DispString(10, y, (uint8_t *)"PID (auto-tuned):");
      y += BYTE_HEIGHT + 2;
      GUI_SetColor(COLOR_TEXT);
      sprintf(str, "  Kp=%.1f  Ki=%.2f  Kd=%.1f",
              (double)Profile_GetLeaded()->pidKp,
              (double)Profile_GetLeaded()->pidKi,
              (double)Profile_GetLeaded()->pidKd);
      _GUI_DispString(10, y, (uint8_t *)str);

      // Pin config
      y += BYTE_HEIGHT + 8;
      GUI_SetColor(YELLOW);
      _GUI_DispString(10, y, (uint8_t *)"Pin Config:");
      y += BYTE_HEIGHT + 2;
      GUI_SetColor(COLOR_TEXT);
      sprintf(str, "  SSR: PC12 (PS-ON)  Active: %s", SSR_ACTIVE_LOW ? "LOW" : "HIGH");
      _GUI_DispString(10, y, (uint8_t *)str);
      y += BYTE_HEIGHT + 2;
      _GUI_DispString(10, y, (uint8_t *)"  TC1 Board:   PA15 CS  (FIL-DET)");
      y += BYTE_HEIGHT + 2;
      _GUI_DispString(10, y, (uint8_t *)"  TC2 Ambient: PC11 CS  (UART4 RX)");

      // Live temps
      y += BYTE_HEIGHT + 8;
      GUI_SetColor(GREEN);
      float bt = MAX6675_GetFilteredTemp(TC_BOARD);
      float at = MAX6675_GetFilteredTemp(TC_AMBIENT);
      sprintf(str, "Board: %.1fC   Ambient: %.1fC", (double)bt, (double)at);
      _GUI_DispString(10, y, (uint8_t *)str);

      // SSR Test button
      if (ssrTestActive)
      {
        GUI_SetColor(RED);
        GUI_FillRect(ssrRect.x0, ssrRect.y0, ssrRect.x1, ssrRect.y1);
        GUI_SetColor(WHITE);
        _GUI_DispStringInRect(ssrRect.x0, ssrRect.y0, ssrRect.x1, ssrRect.y1,
                              (uint8_t *)"SSR ON - TESTING (3s)");
      }
      else
      {
        GUI_SetColor(GREEN);
        GUI_DrawRect(ssrRect.x0, ssrRect.y0, ssrRect.x1, ssrRect.y1);
        GUI_SetColor(COLOR_TEXT);
        _GUI_DispStringInRect(ssrRect.x0, ssrRect.y0, ssrRect.x1, ssrRect.y1,
                              (uint8_t *)"Touch here to TEST SSR (3 sec)");
      }

      // Exit hint
      GUI_SetColor(COLOR_TEXT);
      _GUI_DispString(5, ssrRect.y0 - BYTE_HEIGHT - 5, (uint8_t *)"Touch top half to exit");
    }

    loopProcess();
  }

  // Safety: ensure SSR off
  GPIO_SetLevel(SSR_PIN, SSR_ACTIVE_LOW ? 1 : 0);
}
