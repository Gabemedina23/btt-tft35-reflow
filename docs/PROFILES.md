# Reflow Profile Guide

## Profile Format

Profiles are stored as JSON files on the SD card in the `/profiles/` directory.

```json
{
  "name": "Leaded Sn63/Pb37",
  "description": "Standard leaded solder paste",
  "version": 1,
  "stages": [
    {
      "name": "Preheat",
      "target_temp": 150,
      "ramp_rate": 2.0,
      "hold_time": 0
    },
    {
      "name": "Soak",
      "target_temp": 183,
      "ramp_rate": 0.7,
      "hold_time": 60
    },
    {
      "name": "Reflow",
      "target_temp": 230,
      "ramp_rate": 2.0,
      "hold_time": 0
    },
    {
      "name": "Peak",
      "target_temp": 230,
      "ramp_rate": 0.0,
      "hold_time": 15
    },
    {
      "name": "Cool",
      "target_temp": 50,
      "ramp_rate": -3.0,
      "hold_time": 0
    }
  ],
  "pid": {
    "kp": 200.0,
    "ki": 5.0,
    "kd": 1000.0
  },
  "safety": {
    "max_temp": 260,
    "stage_timeout_multiplier": 3.0
  }
}
```

## Field Descriptions

### Stage Fields

| Field | Type | Unit | Description |
|-------|------|------|-------------|
| `name` | string | -- | Display name for the stage |
| `target_temp` | float | C | Target temperature at end of stage |
| `ramp_rate` | float | C/s | Desired temperature change rate. Positive = heating, negative = cooling, 0 = hold |
| `hold_time` | int | seconds | Time to hold at target_temp after reaching it. 0 = advance immediately |

### PID Fields

| Field | Type | Default | Description |
|-------|------|---------|-------------|
| `kp` | float | 200.0 | Proportional gain. Higher = more responsive but may overshoot |
| `ki` | float | 5.0 | Integral gain. Higher = eliminates steady-state error faster |
| `kd` | float | 1000.0 | Derivative gain. Higher = dampens oscillations |

### Safety Fields

| Field | Type | Default | Description |
|-------|------|---------|-------------|
| `max_temp` | float | 260 | Absolute max temperature. Heater off immediately if exceeded |
| `stage_timeout_multiplier` | float | 3.0 | Stage aborts if it takes longer than expected_time * multiplier |

## Standard Reflow Profiles

### IPC/JEDEC J-STD-020 Guidelines

#### Leaded (Sn63/Pb37)
- Liquidus temperature: 183C
- Peak temperature: 225-235C (target 230C)
- Time above liquidus: 45-75 seconds
- Peak hold: 10-20 seconds within 5C of peak
- Cooldown rate: max 6C/s (target 3C/s)
- Total profile: ~4-5 minutes

#### Lead-Free (SAC305 = Sn96.5/Ag3.0/Cu0.5)
- Liquidus temperature: 217C
- Peak temperature: 245-255C (target 250C)
- Time above liquidus: 40-70 seconds
- Peak hold: 10-20 seconds within 5C of peak
- Cooldown rate: max 6C/s (target 3C/s)
- Total profile: ~5-6 minutes

## PID Tuning Guide

Toaster ovens have very different thermal characteristics than 3D printer hotends:

| Property | 3D Printer Hotend | Toaster Oven |
|----------|-------------------|--------------|
| Thermal mass | ~5g | 500g-2kg |
| Heating rate | 5-10 C/s | 1-3 C/s |
| System lag | 2-5 seconds | 15-45 seconds |
| Typical Kp | 20-30 | 100-300 |
| Typical Ki | 1-3 | 2-10 |
| Typical Kd | 50-150 | 500-2000 |

### Tuning Process

1. **Start with conservative values:** Kp=150, Ki=3, Kd=800
2. **Run a test profile** (just preheat to 150C, hold, then cool)
3. **If oscillating:** Reduce Kp, increase Kd
4. **If too slow to respond:** Increase Kp
5. **If steady-state error:** Increase Ki (slowly)
6. **If overshoot at peak:** Increase Kd, reduce ramp rate

### Control Strategy by Stage

| Stage | Strategy | Notes |
|-------|----------|-------|
| Preheat | Full power (bang-bang) until near target, then PID | Get there fast |
| Soak | PID with ramp rate limiting | Controlled ramp is critical |
| Reflow | PID with feed-forward | Anticipate power needs for ramp |
| Peak Hold | Pure PID | Tight control at peak |
| Cooldown | Heater OFF, fan ON (bang-bang) | Get below liquidus fast |

## Creating Custom Profiles

### Via Touchscreen
1. Menu > Profiles > New Profile
2. Set name, number of stages
3. For each stage: set target temp, ramp rate, hold time
4. Save to SD card

### Via SD Card
1. Create a JSON file following the format above
2. Save to `/profiles/` directory on SD card
3. Insert SD card, profile appears in menu

### Via WiFi (with ESP32 bridge)
1. Connect to the controller's web interface
2. Upload profile JSON or use the visual editor
3. Profile is saved to SD card automatically

## Tips

- **First run:** Use the leaded profile (lower temps) to test your setup
- **Insulation matters:** Better oven insulation = faster ramp, less power, better control
- **Thermocouple placement:** Near the PCB, not touching elements. Temp at the PCB is what matters
- **Preheat the oven:** Let the oven sit at room temp with door closed for consistent starting conditions
- **Board size matters:** Large boards need slower ramp rates to avoid thermal gradients
- **Watch the cooldown:** Too fast = thermal shock = cracked solder joints. 3C/s is a good target
