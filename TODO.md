# Reflow Oven TODO

## In Progress
- [ ] **Fine-tune campaign (2026-08-17):** SD card cleared AM (old logs → `logs/2026-08-17-sd-archive/` — ⚠️ CORRECTION: those 3 files are md5-identical to `logs/2026-05-26-sac305/`, i.e. a re-archive of the May-26 session, not new data; the earlier "not dupes" note compared the wrong baseline). **2 genuinely new SAC305 runs recorded + pulled same day → `logs/2026-08-17-fresh-runs/`** (both truncated mid-Cool — let the oven finish cooling before pulling the card next time). **Full trend analysis → [`docs/2026-08-17-log-trend-analysis.md`](./docs/2026-08-17-log-trend-analysis.md)**: no drift vs May-26, tight A/B repeatability, TAL/peak in spec; soak genuinely short (29–55s in-band) but it's a passive coast at ~0% duty toward an unreachable 200°C target — more time alone won't fix; reflow ramp actuator-limited (full-duty ceiling 0.12–0.22°C/s above 180°C — the "~0.7°C/s" note below is the 120–150°C band, not high temps).
- [x] **✅ RESOLVED 2026-08-17 — firmware/repo drift reconciled.** Real source found: `firmware-base/TFT/src/User/Reflow/` (the gitignored buildable tree) — provenance PROVEN by md5: SD card's flashed `.CUR` == `firmware-base/.pio/build/` binary (`7ac7de55…`), no post-build source edits. Drift = the entire May-26 session (autotune PID 4.1/0.03/146.2, lowered SAC305 200/60+235, per-row f_sync, Heat & Hold mode) applied only in the build tree. All 4 files synced into tracked `firmware/src/Reflow/` (now byte-identical), vendor-file hooks captured as `firmware/vendor-integration.patch` (upstream pinned at `3c46b69`), rebuild recipe + **sync rule** in `firmware/README.md` — that rule is the root-cause fix, follow it after every flashed change.
- [ ] **▶️ ACTIVE-SOAK FIRMWARE BUILT + STAGED 2026-08-17 (commit a9e4b0f) — awaiting flash + test run.** Soak changed 200°C/60s → **170°C/90s** (reachable target = PID actively holds in the J-STD band after the coast-overshoot falls back; Reflow/Peak/cutoff untouched). Built clean (87.5% flash), new `BIGTREE_GD_TFT35_V3.0_E3.28.x.bin` (md5 `52a38f8c…`) **copied to the SD card root, checksum-verified** — move card to oven, power-cycle, TFT auto-flashes + renames to .CUR. Next run logs as **RFLOW_02** (00/01 from today are still on card, already archived). **Validate on next run:** time in 150–180°C band ≥60s, soak duty NONZERO in the hold phase, overshoot peak ≤~185°C, no sag below ~160°C during hold, TAL/peak unchanged (45–90s / 235–250°C). ⚠️ **Flash caveat:** only the `.bin` was staged; project memory says the BTT bootloader wants `.bin` + `config.ini` + `TFT35/` all present (that note may date from initial bringup — config/assets are already on the device). **If the flash doesn't take, the run will show the OLD soak signature (0% duty all soak)** — then rename `config.ini.CUR`→`config.ini` and `TFT35.CUR`→`TFT35` on the card and re-flash. RampRate cosmetic fix (2.0→0.5–0.6) still pending, ride along with a future change.
- [x] **Test reflow with lowered profile** — VALIDATED 2026-05-26 (RFLOW_02). Peak 239.7°C, TAL 51s, full 5-stage profile completed cleanly. Log since deleted; surviving reflow logs in `~/claude/archive/2026-05-reflow-logs/`.
- [ ] **(Cosmetic) Lower Reflow stage rampRate** from 2.0°C/s to 0.8°C/s in reflow_profile.c — oven physically can't ramp faster than ~0.7°C/s at high temps. No functional impact, just makes the spec match reality.
- [ ] **Servo-controlled oven door** — auto crack/open/close per stage (see Future section)
- [ ] **Replace SD card** — current 64 GB card showing flaky USB enumeration after pull-cycles. BTT recommends ≤8 GB. Replace before extended logging.

## Completed (2026-05-26)
- [x] **New fine-wire patch K-type TC installed** — Slice BN thermal paste + kapton on sacrificial coupon top, fiberglass-encapsulated wire run. Mounting verified by paste-melt test.
- [x] **TCAL.DAT removed, calibration NOT rebuilt** — paste-melt test confirmed TC reads accurately at 217°C without calibration. The 6-point TCAL was patching a problem (camera-derived misdiagnosis) that didn't exist. No calibration needed.
- [x] **TC validated via SAC305 paste-melt** — TC reads 215-218°C when paste actually melts (SAC305 eutectic 217°C). Accurate to ±2°C at the temp that matters.
- [x] **Camera unreliability confirmed** — Thermal Master over-reads in oven (pixel-area averaging + cal bias), under-reads water surface. Not usable as truth source.
- [x] **Heat & Hold mode added** — user-selectable target 50-250°C, replaces fixed Burn-In, with target+30°C safety cutoff. See memory `project_reflow_oven.md`.
- [x] **PID values updated to autotune output** — Kp=4.1, Ki=0.03, Kd=146.2 (was 0.69/0.0028/42.5). Applied across all heating modes.
- [x] **SD log durability fix** — f_sync now runs every row (was every 10). Lost-log recovery from prior FAT corruption: `temp/rflow_recovered/RFLOW_00-10.CSV` (since deleted; surviving logs in `~/claude/archive/2026-05-reflow-logs/`).

## Completed (2026-05-24)
- [x] Validated TC reading vs thermal camera (Thermal Master) — board TC tracked ambient TC ±2 °C during burn-in while camera showed +70 °C delta on board surface
- [x] Root-caused TC issue: M6 ring-terminal probe-style TC has wrong thermal characteristics for PCB surface measurement (slow response, no exposed bead, steel-braid heat-sink). Paste + kapton + top-mount alone can't fix it.
- [x] Established calibration coupon methodology (thermal twin) — saved to memory `project_reflow_oven.md`. Sacrificial populated reject per PCB design, top-surface TC bonding via Slice BN paste + kapton (silicone-free, 360 °C continuous), ≥2 cm flat wire run, dual-TC role split (chamber PID vs board profile).
- [x] CSV logs pulled and archived: `archive/2026-05-reflow-logs/RFLOW_00_thermalcam_run.csv` (pre-paste), `archive/2026-05-reflow-logs/RFLOW_01_post_paste.csv` (post-paste, same flawed M6 ring TC)

## Completed (2026-04-13)
- [x] SAC305 profile lowered to minimum spec: soak 200°C/60s, peak 235°C/10s, cutoff 230°C, maxTemp 255°C
- [x] Analyzed RFLOW_03 (successful full reflow), RFLOW_04 (warm start abort), RFLOW_05 (door-opened abort)
- [x] Identified warm-start problem: thermal mass saturation reduces ramp rate above 220°C
- [x] Identified door-opening problem: 5°C coast margin not enough if door bleeds heat during ramp
- [x] SAC305 paste specs researched: 235°C min peak, 40-90s TAL, 10s min at peak

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
