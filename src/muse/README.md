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
