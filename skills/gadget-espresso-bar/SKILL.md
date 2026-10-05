---
name: gadget-espresso-bar
description: >-
  Drive a Muse-enabled home espresso bar over the LAN through Muse Home Link:
  a grind-by-weight grinder and a brew-by-weight shot controller, both running
  custom firmware with a tiny HTTP API. Dose the grinder, set shot yield
  targets, and read live weight / last-shot summaries. Use when the user asks
  to grind, dose, pull a shot, or check the state of their espresso setup.
---

# Espresso Bar (Muse-enabled grinder + shot controller)

Two ESP32 boards on the home LAN, both flashed with the `muse-gadget-wifi`
builds. Muse reaches them through Home Link — no cloud, no app needed.

## Identify the Devices

| Board | Firmware | mDNS | `/status` firmware id |
|---|---|---|---|
| Grinder (Eureka, ESP32-S3) | [MuseGrinder](https://github.com/avidan/MuseGrinder) `muse-gadget-wifi` | `musegrinder.local` | `musegrinder-muse/1.0` |
| Shot controller (Acaia scale link, ESP32) | [MuseCoffeeScale](https://github.com/avidan/MuseCoffeeScale) `muse-gadget-wifi` | `shotstopper-grinder.local` | `shotstopper-muse/1.0` |

Confirm a board before driving it: `GET http://<host>/status` must return the
expected `firmware` id. If mDNS doesn't resolve, find the IP from the router's
DHCP list (both boards also print it on serial at boot). First boot creates a
setup AP (`MuseGrinder-Setup` / `shotStopper-Setup`) for WiFi provisioning.

## Prerequisites

- Both boards flashed with the `muse-gadget-wifi` builds and on the same LAN
  as the Home Link.
- Acaia scale connected to the shot-controller board (`scale_connected: true`
  in `/status`); load cell calibrated on the grinder.
- The HTTP APIs carry **no authentication** — LAN trust only. Never expose
  these endpoints beyond the home network.

## API Reference

### Grinder — `http://musegrinder.local`

- `GET /status` → `{"grinding":bool,"weight_g":float,"target_g":float,"mode":"weight","last_result":"success|overshoot|max_pulses|timeout|error|unknown","firmware":"musegrinder-muse/1.0"}`
- `POST /target` body `{"g": 18.5}` → starts a weight-based grind (1–100g).
  `200 {"target_g":18.5,"started":true}` · `400` out of range · `409` grind already active.
- `POST /stop` → stops the active grind. `409` if none active.
- `GET /last` → `{"valid":bool,"final_weight_g":float,"target_g":float,"result":"..."}`

### Shot controller — `http://shotstopper-grinder.local`

- `GET /status` → `{"scale_connected":bool,"brewing":bool,"weight_g":float,"goal_g":int,"shot_timer_s":float,"firmware":"shotstopper-muse/1.0"}`
- `POST /target` body `{"g": 36}` → sets the yield target in whole grams (10–200g),
  persisted to the board's EEPROM and mirrored to its BLE characteristic.
- `GET /last` → `{"valid":bool,"final_weight_g":float,"goal_g":int,"duration_s":float,"end":"weight|time|button|disconnect|unknown"}`

## Workflows

### Dose the grinder

1. `GET` grinder `/status`. Proceed only if `grinding` is false.
2. `POST /target` with the dose in grams.
3. Poll `GET /status` every ~2s until `grinding` is false (typical grind: 5–15s;
   give up after 60s and report a timeout).
4. `GET /last` and report `final_weight_g` vs the target. Within ±0.1g is a
   clean dose; anything wider is worth a re-grind.

### Pull a shot at a ratio

1. Get the dose: grinder `GET /last` → `final_weight_g` (or ask the user).
2. Yield target = dose × ratio (e.g. 18.5g at 1:2 → 37g, rounded to whole grams
   for the shot controller).
3. `POST` the yield to the shot controller `/target`.
4. Tell the user the board is armed: it auto-tares, auto-starts the timer, and
   stops the pump at the target yield once they lift the paddle / press brew.
5. After the shot, `GET /last` on the shot controller and report yield,
   duration, and end reason.

### Check the bar's state

`GET /status` on both boards. Report grinding/brewing state, live weights,
targets, scale link, and the last result of each.

## Verify the Result

- After any actuation, confirm via `/last` — report whether the final weight
  is measured (from the board) and never infer a completed grind from a
  `200 {"started":true}` alone.
- If `/status` shows `grinding: true` for more than 60s, or `last_result` is
  `timeout`/`error`, report it and stop — do not re-fire the grinder until the
  user has checked the machine.

## Safety

- Starting a grind spins a real motor; setting a yield target arms a real pump
  cutoff. **Never actuate without the user's explicit request.** The device
  existing on the network does not authorize its actuation.
- Do not change a target mid-grind or mid-shot.
- Do not retry a failed grind more than once without the user checking the
  hopper, chute, and scale.

## Limits

- LAN only, no auth — keep these endpoints off the internet.
- The shot-controller target is whole grams (10–200g); the grinder accepts
  1–100g with one decimal.
- Both firmwares are experimental builds — verify behavior on the boards
  before trusting them in a morning routine.
- No OTA, no reflashing, no WiFi reprovisioning through this skill.

## Sources

- Grinder firmware + PR: [avidan/MuseGrinder](https://github.com/avidan/MuseGrinder)
- Shot-controller firmware + PR: [avidan/MuseCoffeeScale](https://github.com/avidan/MuseCoffeeScale)
- Muse Gadgets SDK (skills format): [facebookincubator/muse-gadget-sdk](https://github.com/facebookincubator/muse-gadget-sdk)
