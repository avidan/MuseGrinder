#pragma once

class GrindController;
class WeightSensor;
class ProfileController;

// Muse gadget add-on: WiFi + HTTP API so Muse (via Home Link on the LAN)
// can dose the grinder and read grind state. The grind engine is untouched.
//
// Wiring (2 lines in src/main.cpp):
//   setup(), at the end : museWifiSetup(&grind_controller, hardware_manager.get_load_cell(), &profile_controller);
//   loop(), first line   : museWifiLoop();
void museWifiSetup(GrindController* gc, WeightSensor* ws, ProfileController* pc);
void museWifiLoop();

// POST /dose queues the next dose here; the UI task applies it to the
// selected profile (LVGL and profile edits belong to that task). Returns
// true once per queued dose.
bool museTakePendingDose(float* grams);

// Full TX power while a WiFi firmware upload runs (the motor is idle then);
// a 3MB upload at the reduced power can stall long enough to time out.
void museWifiSetUploadActive(bool active);
