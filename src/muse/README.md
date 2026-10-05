# Muse gadget add-on

Makes the grinder reachable by Muse over the LAN (via a Muse Home Link):
dose targets, live status, and last-grind summaries over HTTP. The grind
engine, predictive algorithm, and UI are untouched.

## Wiring

Two lines in `src/main.cpp`:

```cpp
#include "muse/muse_wifi.h"   // with the other includes

// end of setup():
museWifiSetup(&grind_controller, hardware_manager.get_load_cell());

// first line of loop():
museWifiLoop();
```

Plus one line in `platformio.ini` `lib_deps`:

```
tzapu/WiFiManager@^2.0.17
```

## First boot

Join the `MuseGrinder-Setup` AP, pick your WiFi network. Thereafter:
`http://musegrinder.local`.

## API

| Method | Path | Effect |
|---|---|---|
| GET | `/status` | `{grinding, weight_g, target_g, mode, last_result, firmware}` |
| POST | `/target` | Body `{"g": 18.5}` — starts a weight-based grind (1–100g); 409 if a grind is already active |
| POST | `/stop` | Stops the active grind |
| GET | `/last` | `{valid, final_weight_g, target_g, result}` for the last completed grind |

## Notes

- Uncompiled here (no ESP32 toolchain on this machine) — verify the
  PlatformIO build before flashing.
- `start_grind()` is called from the Core 1 loop task, the same context the
  touch UI already uses.
- Next: the Muse gadget skill (markdown) teaching Muse to drive this API
  alongside the GS3's shotStopper board.

## Jolly on the grinding screen (`muse_mascot.h` / `muse_mascot.cpp`)

The grinding screen's main visual is the real Jolly — Meta's Muse mascot —
played from frames converted out of the gadget SDK's `esp32/avatar/jollybot.gif`.

- **Idle:** the authentic happy-bounce loop (25 frames @ ~120ms).
- **Straining** (motor running): the same frames with a side-to-side shudder,
  squash-and-stretch pulse, and flickering motion lines — the gag being the
  real grounds falling from the chute. The mascot itself stays clean: no
  feces drawn, ever.
- **Relieved:** back to the happy loop on grind complete / timeout.

### Build-time art pipeline (read this before flashing)

The Jolly artwork is **Meta-copyrighted** — the gadget SDK's Apache license
does not cover it, so the frames are **never committed** to this repo.

- `tools/gen_jolly.py` (committed, contains no art) converts `jollybot.gif`
  from your local `muse-gadget-sdk` clone into `src/muse/jolly_anim.c/h`.
- `tools/build-scripts/pre_jolly.py` runs it automatically on every build
  (registered in `platformio.ini` `extra_scripts`).
- `src/muse/jolly_anim.c` and `jolly_anim.h` are in `.gitignore`.
- The script finds the GIF via `$JOLLY_GIF`, or `<repo>/../muse-gadget-sdk`,
  or `~/workspace/coffee-gadgets/muse-gadget-sdk`. If it can't find it, the
  build fails with instructions — set the env var and rebuild.
- 25 frames @ 112x112 RGB565 ≈ 612 KiB of flash. Tune `FRAME_W`/`FRAME_STEP`
  in `gen_jolly.py` if the app partition gets tight.

Wiring: `GrindingScreenArc` (default layout) headlines the mascot and swaps
the 200px progress arc for a slim progress bar; `GrindingScreenChart` gets
the mascot above the profile label (chart shrunk 140→100px to fit).
`GrindingScreen::set_straining(bool)` fans out to both layouts; `UIManager`
sets it true on `GRINDING`, false on `GRIND_COMPLETE` / `GRIND_TIMEOUT`.
Animation runs on an `lv_timer` (100ms); `tick()` early-outs while hidden.
