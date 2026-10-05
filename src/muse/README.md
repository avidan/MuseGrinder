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
