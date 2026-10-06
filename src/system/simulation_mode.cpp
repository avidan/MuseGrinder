#include "simulation_mode.h"
#include "../config/constants.h"
#include <Arduino.h>
#include <Preferences.h>

namespace {
constexpr const char* kPrefsNamespace = "simulation";
constexpr const char* kPrefsKey = "enabled";
bool s_enabled = DEBUG_ENABLE_LOADCELL_MOCK != 0;
}  // namespace

namespace SimulationMode {

void load() {
#if DEBUG_ENABLE_LOADCELL_MOCK
    s_enabled = true;
#else
    s_enabled = saved();
#endif
    if (s_enabled) {
        LOG_BLE("[SIMULATION] Simulation mode active - motor relay disabled, simulated load cell\n");
    }
}

bool enabled() {
    return s_enabled;
}

bool saved() {
    Preferences prefs;
    if (!prefs.begin(kPrefsNamespace, true)) {
        return false;  // namespace not created yet
    }
    bool value = prefs.getBool(kPrefsKey, false);
    prefs.end();
    return value;
}

void save_and_restart(bool simulate) {
    Preferences prefs;
    if (prefs.begin(kPrefsNamespace, false)) {
        prefs.putBool(kPrefsKey, simulate);
        prefs.end();
    }
    LOG_BLE("[SIMULATION] Simulation mode %s - restarting\n", simulate ? "enabled" : "disabled");
    Serial.flush();
    delay(100);
    ESP.restart();
}

}  // namespace SimulationMode
