# Reflow Oven TODO

## In Progress
- [ ] **Run thermocouple calibration** — Run calibration with aluminum foil wire rack setup, verify readings match reference thermometer
- [ ] **Test reflow with calibrated sensor** — After calibration, do a test reflow on a sacrificial board to verify temps are correct

## Completed (2026-04-12)
- [x] SAC305 peak temp reduced from 250°C to 240°C (47µF electrolytic was steaming at 237°C)
- [x] Data-driven ramp strategy: split PREHEAT (100% until cutoff) and RAMP (100%→75%→50%→PID cascade)
- [x] Heater cutoff with thermal coast: preheat cuts at 140°C→coasts to 150°C, reflow cuts at 235°C→coasts to 240°C
- [x] Stage timeout multiplier increased to 15x (oven ramp rate at high temp is ~0.3°C/s vs 2.0°C/s profile spec)

## Completed (2026-04-11)
- [x] SD card logging during calibration (CSV with temp, duty cycle, events, swing tracking)
- [x] Overshoot door prompts during heating/stable phases (not just between steps)
- [x] Four-phase ramp-up: 100% kick (5s) → 50% cruise → 20% taper → 10% approach
- [x] PID values re-tuned from autotune data with aluminum foil wire rack (Kp=0.69, Ki=0.0028, Kd=42.5)
- [x] Heat diffuser testing: steel tray → aluminum foil wire rack (30% tighter oscillation, 40% less overshoot)
- [x] Ambient thermocouple fixed (loose wires on UART4 header)
- [x] Four-phase ramp applied to reflow controller (was only in calibration/autotune, caused thermal runaway)
- [x] README: added "Heat Management" section documenting steel tray vs aluminum foil with data

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
- [ ] **Servo-controlled oven door** — auto crack/open/close per stage
  - Pin: PA8 (EXP2 pin 1, TIM1_CH1 hardware PWM)
  - Power: 5V + GND from EXP2 header
  - Servo: SG90 or MG90S micro servo (need to order)
  - Mount: 3D-printed bracket on oven door hinge
  - Door positions per stage: Preheat=closed, Soak=cracked 15-20° (bleed overshoot), Reflow=closed, Cool=wide open 90°
  - Add `doorAngle` field to ReflowStage struct
  - Firmware: servo.c/h — init TIM1_CH1 at 50Hz, set angle 0-180°
- [ ] Fix screen flicker (partial redraw instead of full clear)
- [ ] Profile editor on touchscreen
- [ ] Re-run autotune if switching between steel tray and aluminum foil setups
