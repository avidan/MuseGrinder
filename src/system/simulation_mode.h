#pragma once

// Simulation mode: run the grinder UI and grind algorithm with no motor or
// load cell attached. The simulated load cell (MockHX711Driver) adds weight
// while the "motor" runs, and the motor relay is never driven.
//
// The setting lives in NVS and is read once at boot, because the load cell
// driver and motor output are chosen during hardware init. Changing it
// requires a restart. Mock builds (DEBUG_ENABLE_LOADCELL_MOCK) are always
// simulated.
namespace SimulationMode {

// Read the saved setting. Call once in setup(), before hardware init.
void load();

// Whether this boot is simulated.
bool enabled();

// Whether simulation is saved for the next boot (may differ from enabled()).
bool saved();

// Save the setting for the next boot and restart the device.
void save_and_restart(bool simulate);

}  // namespace SimulationMode
