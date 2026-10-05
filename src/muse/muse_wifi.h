#pragma once

class GrindController;
class WeightSensor;

// Muse gadget add-on: WiFi + HTTP API so Muse (via Home Link on the LAN)
// can dose the grinder and read grind state. The grind engine is untouched.
//
// Wiring (2 lines in src/main.cpp):
//   setup(), at the end : museWifiSetup(&grind_controller, hardware_manager.get_load_cell());
//   loop(), first line   : museWifiLoop();
//
// Requires the WiFiManager library (added to platformio.ini lib_deps).
void museWifiSetup(GrindController* gc, WeightSensor* ws);
void museWifiLoop();
