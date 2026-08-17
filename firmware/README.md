# Reflow firmware — source layout + build truth

**Reconciled 2026-08-17.** The flashed firmware's full provenance chain is verified:
SD card `BIGTREE_GD_TFT35_V3.0_E3.28.x.CUR` md5 `7ac7de5575861d45bbc13d4275b0fafd` == `firmware-base/.pio/build/BIGTREE_GD_TFT35_E3_V3_0/*.bin` (built 2026-05-26 from the `firmware-base/` tree, no source edits after build).

## Layout

- **`firmware/src/Reflow/`** (THIS repo, tracked) — canonical copy of our reflow module. As of 2026-08-17 it is byte-identical to the build tree's copy.
- **`firmware-base/`** (gitignored) — full buildable tree: a clone of [bigtreetech/BIGTREETECH-TouchScreenFirmware](https://github.com/bigtreetech/BIGTREETECH-TouchScreenFirmware) at upstream commit `3c46b699ca7c265e3ce5c6bf5964f956a6bdb36a`, with (a) our `TFT/src/User/Reflow/` module dropped in and (b) 6 vendor files modified to hook it in (`includes.h`, `Menu/MainPage.c`, `API/HW_Init.c`, `API/ModeSwitching.c`, `API/menu.c`, `buildroot/scripts/pre_install_dependencies.py`) + custom `logo.bmp`.
- **`firmware/vendor-integration.patch`** (tracked) — `git diff --binary` of ALL vendor-file modifications in `firmware-base/`, so the complete flashed firmware is reconstructible from this repo alone:
  ```
  git clone https://github.com/bigtreetech/BIGTREETECH-TouchScreenFirmware.git firmware-base
  git -C firmware-base checkout 3c46b699ca7c265e3ce5c6bf5964f956a6bdb36a
  git -C firmware-base apply --binary ../firmware/vendor-integration.patch   # run from repo root: adjust path
  cp firmware/src/Reflow/* firmware-base/TFT/src/User/Reflow/   # plus the .h files already identical in both trees
  pio run -e BIGTREE_GD_TFT35_E3_V3_0   # in firmware-base/
  ```

## ⚠️ The sync rule (what caused the 2026-05→08 drift)

Builds happen in `firmware-base/` (gitignored) — edits made there are invisible to git. The May-26 session's changes (autotune PID 4.1/0.03/146.2, lowered SAC305 profile 200/60 + 235 peak, per-row `f_sync`, Heat & Hold mode) lived only in the build tree for 3 months while the tracked copy went stale.

**Rule: after ANY firmware change that gets flashed — before ending the session:**
1. `diff -rq firmware/src/Reflow firmware-base/TFT/src/User/Reflow` → sync + commit.
2. If vendor files were touched: regenerate `firmware/vendor-integration.patch` (`git -C firmware-base diff --binary > firmware/vendor-integration.patch`) → commit.
