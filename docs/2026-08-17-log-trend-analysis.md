# Reflow log trend analysis — 2026-08-17

Agent-run analysis of all 68 log files (47 unique sessions by content hash): the two fresh SAC305 runs from today (`logs/2026-08-17-fresh-runs/`), the May-26 validated set, and the April tuning campaign. Cross-checked against `firmware/src/Reflow/` source and git history.

## Data corrections

- **`logs/2026-08-17-sd-archive/` is NOT new data** — all 3 files are byte-identical (md5) to `logs/2026-05-26-sac305/RFLOW_00/01/02`. The SD card had been carrying the May-26 session logs all along. (The earlier "not dupes" note compared against the wrong baseline — the unrelated April numbered series in `logs/`.)
- The only genuinely new data = today's 2 fresh runs. **Both are truncated mid-Cool** (card pulled / logging ended before cooldown finished) — Preheat/Soak/Reflow/Peak data intact, cooldown partial. For future runs: let the oven finish cooling before pulling the card.

## ⚠️ Critical: the oven runs firmware this repo does not contain

Committed `reflow_profile.c` (unchanged since Apr 12, c88c97a) says Soak 217°C/90s, Reflow/Peak 240°C/20s. Every real run May-26 → today actually ran **Soak 200°C/60s, Reflow/Peak 235°C, cutoff ≈230°C, ~10s peak hold** — matching the Apr-13 "lowered to minimum spec" TODO note that never landed as a commit. The Heat & Hold mode (adjustable 50–250°C) doesn't exist in committed `reflow_menu.c` either (only fixed-150°C Burn-In). **Editing the repo's profile will not change oven behavior until the real flashed source is located and reconciled.**

## Key metrics (SAC305 runs)

| Metric | Fresh A (today) | Fresh B (today) | May-26 validated | J-STD target |
|---|---|---|---|---|
| Preheat ramp | 1.25°C/s | 1.27°C/s | 1.54°C/s | 1–3°C/s ✓ |
| Time in 150–180°C soak band | 55s | 50s | 29s | 60–120s ✗ |
| Soak mean duty | 4.3% | 2.2% | 2.3% | (passive coast) |
| Reflow ramp achieved | 0.575°C/s | 0.582°C/s | 0.664°C/s | (coded spec 2.0) |
| Peak | 238.5°C | 238.0°C | 239.7°C | 235–250°C ✓ |
| TAL ≥217°C | 83s | 78s | 52s | 45–90s ✓ |

Back-to-back repeatability (A vs B): rates within 1–2%, peaks within 0.5°C. Vs May-26: ~13–15% slower reflow ramp + longer TAL, consistent with today's cooler start (42–43°C vs 49.6°C ambient) — **behavior has held, no drift.**

## Plant characterization (Heat & Hold 220°C session)

Full-duty rise rate by band: 30–80°C: 0.50 · 80–120: 0.88 · 120–150: 0.68 · 150–180: 0.44 · 180–200: **0.22** · 200–215: **0.12°C/s**. This corrects the TODO's "~0.7°C/s at high temps" — that number is the 120–150°C band; above 180°C the ceiling is 3–6× lower. Thermal lag is large: duty cut 100%→0 at board 210°C still coasted +24°C over ~18s to 234.2°C (14°C past setpoint) with zero power. No steady-state reached (session manually stopped mid-correction).

## Hypothesis verdicts (Gabe's intuition, tested)

- **(a) "Soak isn't long enough" — CONFIRMED, wrong mechanism.** 29–55s in-band vs 60–120s target, genuinely short. But soak duty is ~0% (80–93% of rows exactly 0): the board coasts on preheat's stored heat toward an unreachable 200°C target. Adding seconds alone won't fix it — the coast flattens out short of target regardless.
- **(b) "Ramp-up isn't fast enough" — CONFIRMED vs the coded 2.0°C/s spec, but NOT fixable by tuning.** The oven is actuator-limited above 180°C (full-duty ceiling 0.12–0.22°C/s); achieved 0.58–0.66°C/s cascade is already better than the isolated ceiling (open question why — possibly coil thermal history). No PID/profile change can make it faster; the lever would be reducing thermal loss (door/insulation) or accepting it.

## Recommendations (ranked)

1. **Prerequisite (high confidence):** find + reconcile the actually-flashed firmware source (Soak 200/60, peak 235, Heat & Hold feature) before any profile edits in git.
2. **Medium:** the pending "rampRate 2.0→0.8" cosmetic fix should be **0.5–0.6°C/s** instead (matches measured reality; value only feeds duration/timeout math).
3. **Optional design choice (medium):** if a true isothermal soak is wanted → active PID hold at a reachable ~165–170°C target instead of the unreachable-by-coast 200°C. Note: SAC305 tolerates ramp-to-spike (effectively what runs today), so this is optional.
4. **No change needed:** TAL, peak, cooldown all in spec, every real run; repeatability tight; no oven-health concerns.

Scratch analysis script preserved this session; raw per-band numbers above. Analysis by circuit-design-expert agent, 2026-08-17.
