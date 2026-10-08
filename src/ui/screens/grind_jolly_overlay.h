#pragma once
#include <lvgl.h>
#include "../../muse/muse_mascot.h"

// Full-screen Jolly that replaces the grinding UI:
//  - straining on the toilet while a grind runs
//  - dancing on the toilet once the grind completes
// It covers everything else; tapping it does what the hidden grind button
// would (stop while grinding, dismiss when complete).
class GrindJollyOverlay {
public:
    using TapHandler = void (*)(void* context);

    void create(TapHandler on_tap, void* context);
    void show_straining();
    void show_dancing();
    void hide();
    // Straining animates only while the motor actually runs; it freezes on
    // pause, settling and between pulses.
    void set_motor_running(bool running) { straining.setFrozen(!running); }

private:
    static void tapped_cb(lv_event_t* e);

    lv_obj_t* overlay = nullptr;
    MuseMascot straining;
    MuseMascot dancing;
    TapHandler on_tap = nullptr;
    void* context = nullptr;
};
