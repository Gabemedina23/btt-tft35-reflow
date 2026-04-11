# Reflow Oven TODO

## In Progress
- [ ] **Run thermocouple calibration** — Tape board TC to PCB with kapton tape, run calibration procedure with meat thermometer, verify readings match
- [ ] **Test reflow with calibrated sensor** — After calibration, do a test reflow on a sacrificial board to verify temps are correct

## Completed (2026-04-11)
- [x] SD card logging during calibration (CSV with temp, duty cycle, events, swing tracking)
- [x] Graduated duty cycle to prevent overshoot (100% >30°C away, 75% 20-30, 50% 10-20, PID <10)
- [x] Door open/close prompts during calibration cooling (CAL_COOLING state)
- [x] Temperature swing tracking during heating (logged as events)

## Completed (2026-04-10)
- [x] Separate profile selection from start (Profiles menu selects, Start button runs selected)
- [x] Multi-point thermocouple calibration system (6 points: ambient to 200°C)
- [x] C/F toggle for reference thermometer input
- [x] Encoder dial support for temperature input (rotate = +/-1, click = confirm)
- [x] SD card persistence for calibration data (TCAL.DAT)
- [x] Piecewise linear correction applied to all board sensor readings
- [x] Auto-load calibration from SD card at boot

## Known Issues
- [ ] Thermocouple reads ~60°C low vs actual temperature (calibration will fix)
- [ ] Board charred on first real reflow (lead-free) — radiant heat from quartz elements heats dark PCB surface much hotter than air temp near the probe
- [ ] Soak stage overshoots target by ~17°C (thermal inertia — graduated duty cycle in calibration may help here too)
- [ ] No profile editor yet (hardcoded profiles)

## Future
- [ ] Add heat diffuser/tray between top elements and board to reduce radiant heat differential
- [ ] Consider adding fan for cooldown phase
- [ ] Profile editor on touchscreen
