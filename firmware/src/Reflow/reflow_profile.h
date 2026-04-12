#ifndef _REFLOW_PROFILE_H_
#define _REFLOW_PROFILE_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

#define PROFILE_MAX_STAGES    8
#define PROFILE_NAME_LEN      32
#define PROFILE_DESC_LEN      64

// A single stage in the reflow profile
typedef struct {
  char     name[PROFILE_NAME_LEN]; // e.g., "Preheat", "Soak", "Reflow", "Peak", "Cool"
  float    targetTemp;             // Target temperature (C) at end of this stage
  float    rampRate;               // Desired ramp rate (C/s). 0 = hold. Negative = cool.
  uint16_t holdTime;               // Seconds to hold at targetTemp after reaching it
  float    heaterCutoff;           // Kill heater at this temp and coast to targetTemp (0 = disabled)
} ReflowStage;

// Complete reflow profile
typedef struct {
  char        name[PROFILE_NAME_LEN];
  char        description[PROFILE_DESC_LEN];
  uint8_t     version;
  uint8_t     numStages;
  ReflowStage stages[PROFILE_MAX_STAGES];

  // PID parameters for this profile
  float       pidKp;
  float       pidKi;
  float       pidKd;

  // Safety parameters
  float       maxTemp;             // Absolute max temp before emergency shutdown
  float       stageTimeoutMult;    // Stage aborts if time > expected * multiplier
} ReflowProfile;

// --- Built-in profiles ---

// Get the default leaded solder profile (Sn63/Pb37)
const ReflowProfile * Profile_GetLeaded(void);

// Get the default lead-free solder profile (SAC305)
const ReflowProfile * Profile_GetLeadFree(void);

// --- Profile storage (SD card) ---

// Load a profile from a JSON file on the SD card
// path: e.g., "profiles/my_profile.json"
// Returns true on success
bool Profile_LoadFromSD(const char *path, ReflowProfile *profile);

// Save a profile to a JSON file on the SD card
bool Profile_SaveToSD(const char *path, const ReflowProfile *profile);

// List available profile files on the SD card
// names: output array of filename strings
// maxCount: maximum profiles to list
// Returns: number of profiles found
uint8_t Profile_ListSD(char names[][PROFILE_NAME_LEN], uint8_t maxCount);

// --- Profile utilities ---

// Calculate expected total duration of a profile (seconds)
uint32_t Profile_GetExpectedDuration(const ReflowProfile *profile, float startTemp);

// Calculate expected duration of a single stage (seconds)
uint16_t Profile_GetStageDuration(const ReflowStage *stage, float startTemp);

// Get the peak temperature in the profile
float Profile_GetPeakTemp(const ReflowProfile *profile);

#ifdef __cplusplus
}
#endif

#endif // _REFLOW_PROFILE_H_
