# Reflow Oven TODO

## In Progress
- [ ] **Run thermocouple calibration** — Run calibration with aluminum foil wire rack setup, verify readings match reference thermometer
- [ ] **Test reflow with calibrated sensor** — After calibration, do a test reflow on a sacrificial board to verify temps are correct

## Completed (2026-04-11)
- [x] SD card logging during calibration (CSV with temp, duty cycle, events, swing tracking)
- [x] Overshoot door prompts during heating/stable phases (not just between steps)
- [x] Four-phase ramp-up: 100% kick (5s) → 50% cruise → 20% taper → 10% approach
- [x] PID values re-tuned from autotune data with aluminum foil wire rack (Kp=0.69, Ki=0.0028, Kd=42.5)
- [x] Heat diffuser testing: steel tray → aluminum foil wire rack (30% tighter oscillation, 40% less overshoot)
- [x] Ambient thermocouple fixed (loose wires on UART4 header)

## Completed (2026-04-10)
- [x] Separate profile selection from start (Profiles menu selects, Start button runs selected)
- [x] Multi-point thermocouple calibration system (6 points: ambient to 200°C)
- [x] C/F toggle for reference thermometer input
- [x] Encoder dial support for temperature input (rotate = +/-1, click = confirm)
- [x] SD card persistence for calibration data (TCAL.DAT)
- [x] Piecewise linear correction applied to all board sensor readings
- [x] Auto-load calibration from SD card at boot

## Known Issues
- [ ] Screen flickers during calibration/autotune (full redraw every 500ms — cosmetic only)
- [ ] No profile editor yet (hardcoded profiles)

## Future
- [ ] Fix screen flicker (partial redraw instead of full clear)
- [ ] Add cooling fan (PA0 on UART4 header available, mount externally)
- [ ] Profile editor on touchscreen
- [ ] Re-run autotune if switching between steel tray and aluminum foil setups
