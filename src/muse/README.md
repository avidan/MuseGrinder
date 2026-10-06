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

No extra libraries — WiFi setup uses a small built-in portal (WiFiManager
was dropped: it needs Arduino's global `Update`, which `NO_GLOBAL_UPDATE`
removes for esp32-flashz, and it exposes an unauthenticated OTA page).

## First boot

WiFi never blocks boot; the grinder works normally offline. If no saved
network connects within 20s of boot, join the open `MuseGrinder-Setup` AP
(captive portal, or browse to `192.168.4.1`) and enter your network. The AP
closes once the grinder joins. Thereafter: `http://musegrinder.local`.

## API

| Method | Path | Effect |
|---|---|---|
| GET | `/status` | `{grinding, weight_g, target_g, mode, last_result, firmware}` |
| POST | `/target` | Body `{"g": 18.5}` — starts a weight-based grind (1–100g); 409 if a grind is already active. A finished grind still on screen is dismissed first |
| POST | `/stop` | Stops the active grind |
| GET | `/last` | `{valid, final_weight_g, target_g, result}` for the last completed grind |

## Notes

- `grinding` is false once a grind reaches COMPLETED/TIMEOUT, even before
  the result screen is dismissed. `/last` is captured at that moment, with
  the cup still on the scale. Stopped (cancelled) grinds don't update `/last`.
- `/target` doesn't check which UI screen is showing (e.g. menu or
  calibration) — Muse should only dose when the grinder is idle.
- Runs in the Arduino `loop()` task, separate from the UI and grind control
  tasks. GrindController serializes start/stop/update with a mutex.
- `/target` returns 503 if the grinder refuses to start (load cell fault or
  scale not calibrated).
- Next: the Muse gadget skill (markdown) teaching Muse to drive this API
  alongside the GS3's shotStopper board.

## Jolly on the grinding screen (`muse_mascot.h` / `muse_mascot.cpp`)

The grinding screen's main visual is the real Jolly — Meta's Muse mascot —
played from frames converted out of the gadget SDK's `esp32/avatar/jollybot.gif`.

- **Idle / done:** the authentic happy dance (15 frames @ 200ms), sitting on
  the same toilet — hearts, waving arms, blinking.
- **Straining** (motor running): 4 frames @ 160ms generated from Jolly's
  neutral pose — Jolly crouched on a toilet (tank behind, seat and bowl in
  front), eyes squeezed shut (`> <`), clenched zigzag mouth, deep-red face,
  flying sweat, pulsing strain veins, and shaking motion lines. The gag is the real grounds falling from the chute; the
  mascot itself stays clean: no feces drawn, ever.
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
- 19 frames @ 112x124 RGB565 ≈ 515 KiB of flash. Tune `FRAME_W`/`FRAME_STEP`
  in `gen_jolly.py` if the app partition gets tight. The straining edits
  (eye/mouth/face boxes) are constants at the top of the script, in the
  GIF's 320px source coordinates.
- Preview what the device will show without flashing (3x animated GIFs,
  decoded back from the RGB565 frames — don't commit these either):
  `python3 tools/gen_jolly.py --out-dir /tmp/jolly --preview /tmp/jolly`

Wiring: `GrindingScreenArc` (default layout) headlines the mascot at ~1.4x
(157x174px) and swaps the 200px progress arc for a slim progress bar;
`GrindingScreenChart` gets the mascot at 1.25x (140x155px) above the profile
label (chart shrunk 140→80px to fit). Scaling is nearest-neighbour so the
pixel art stays crisp.
`GrindingScreen::set_straining(bool)` fans out to both layouts; `UIManager`
sets it true on `GRINDING`, false on `GRIND_COMPLETE` / `GRIND_TIMEOUT`.
Animation runs on an `lv_timer` (50ms) that picks the frame from the tick
clock; `tick()` early-outs while hidden. Scaled players pre-render each new
frame (nearest neighbour) into a PSRAM buffer, so LVGL only blits it.
Letting LVGL scale a 3x image on every refresh pegged core 1 at ~180ms per
UI cycle and starved `loop()` — and with it the Muse HTTP server.

While a grind runs, `GrindJollyOverlay` shows only the straining Jolly at
3x (336x372, edges slightly clipped), toilet on the bottom edge of the
screen; tap to stop. When the grind completes it switches to the dancing
Jolly, same toilet and size, alone on screen, for 5s
(`USER_GRIND_COMPLETE_DISPLAY_MS`) before returning to dose selection; tap
to return sooner.
Timeouts show the normal screen so the error is visible. Both loops use the
same tight crop (`SCENE_CROP`, 112x124) so the scene fills more of the
display and Jolly doesn't jump between them.

**Menu → Tools → Jolly** shows the animation at 2x. Tap Jolly or flip the
Straining switch to swap between the idle and grinding animations.
