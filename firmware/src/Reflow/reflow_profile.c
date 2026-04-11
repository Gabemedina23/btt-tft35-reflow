#include "includes.h"
#include "reflow_profile.h"
#include <math.h>

// =============================================================================
// Built-in Profiles
// =============================================================================

static const ReflowProfile profileLeaded = {
  .name        = "Leaded Sn63/Pb37",
  .description = "Standard leaded solder paste",
  .version     = 1,
  .numStages   = 5,
  .stages = {
    { .name = "Preheat", .targetTemp = 150.0f, .rampRate =  2.0f, .holdTime = 0  },
    { .name = "Soak",    .targetTemp = 183.0f, .rampRate =  0.7f, .holdTime = 60 },
    { .name = "Reflow",  .targetTemp = 230.0f, .rampRate =  2.0f, .holdTime = 0  },
    { .name = "Peak",    .targetTemp = 230.0f, .rampRate =  0.0f, .holdTime = 15 },
    { .name = "Cool",    .targetTemp =  50.0f, .rampRate = -3.0f, .holdTime = 0  },
  },
  .pidKp = 0.69f,
  .pidKi = 0.0028f,
  .pidKd = 42.5f,
  .maxTemp = 260.0f,
  .stageTimeoutMult = 5.0f,
};

static const ReflowProfile profileLeadFree = {
  .name        = "Lead-Free SAC305",
  .description = "Lead-free solder paste (SAC305)",
  .version     = 1,
  .numStages   = 5,
  .stages = {
    { .name = "Preheat", .targetTemp = 150.0f, .rampRate =  2.0f, .holdTime = 0  },
    { .name = "Soak",    .targetTemp = 217.0f, .rampRate =  0.7f, .holdTime = 90 },
    { .name = "Reflow",  .targetTemp = 250.0f, .rampRate =  2.0f, .holdTime = 0  },
    { .name = "Peak",    .targetTemp = 250.0f, .rampRate =  0.0f, .holdTime = 15 },
    { .name = "Cool",    .targetTemp =  50.0f, .rampRate = -3.0f, .holdTime = 0  },
  },
  .pidKp = 0.69f,
  .pidKi = 0.0028f,
  .pidKd = 42.5f,
  .maxTemp = 280.0f,
  .stageTimeoutMult = 5.0f,
};

const ReflowProfile * Profile_GetLeaded(void)
{
  return &profileLeaded;
}

const ReflowProfile * Profile_GetLeadFree(void)
{
  return &profileLeadFree;
}

// =============================================================================
// Profile Utilities
// =============================================================================

uint16_t Profile_GetStageDuration(const ReflowStage *stage, float startTemp)
{
  float tempDelta = fabsf(stage->targetTemp - startTemp);
  float rampRate = fabsf(stage->rampRate);

  uint16_t rampTime = 0;
  if (rampRate > 0.01f)
    rampTime = (uint16_t)(tempDelta / rampRate);

  return rampTime + stage->holdTime;
}

uint32_t Profile_GetExpectedDuration(const ReflowProfile *profile, float startTemp)
{
  uint32_t total = 0;
  float currentTemp = startTemp;

  for (uint8_t i = 0; i < profile->numStages; i++)
  {
    total += Profile_GetStageDuration(&profile->stages[i], currentTemp);
    currentTemp = profile->stages[i].targetTemp;
  }

  return total;
}

float Profile_GetPeakTemp(const ReflowProfile *profile)
{
  float peak = 0.0f;
  for (uint8_t i = 0; i < profile->numStages; i++)
  {
    if (profile->stages[i].targetTemp > peak)
      peak = profile->stages[i].targetTemp;
  }
  return peak;
}

// =============================================================================
// SD Card Profile Storage (placeholder -- needs BTT FatFs integration)
// =============================================================================

// TODO: Implement JSON parsing using a minimal JSON parser (e.g., jsmn)
// The BTT firmware already includes FatFs for SD card access.
// These functions will parse/write the profile JSON format documented in docs/PROFILES.md

bool Profile_LoadFromSD(const char *path, ReflowProfile *profile)
{
  (void)path;
  (void)profile;
  // TODO: Open file via FatFs, parse JSON, populate profile struct
  return false;
}

bool Profile_SaveToSD(const char *path, const ReflowProfile *profile)
{
  (void)path;
  (void)profile;
  // TODO: Serialize profile to JSON, write via FatFs
  return false;
}

uint8_t Profile_ListSD(char names[][PROFILE_NAME_LEN], uint8_t maxCount)
{
  (void)names;
  (void)maxCount;
  // TODO: Scan /profiles/ directory for *.json files
  return 0;
}
