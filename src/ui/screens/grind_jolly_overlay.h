#pragma once
#include <lvgl.h>
#include "../../muse/muse_mascot.h"

// Full-screen straining Jolly shown while a grind runs. Covers everything
// else; tapping anywhere stops the grind (the stop button it hides).
class GrindJollyOverlay {
public:
    using TapHandler = void (*)(void* context);

    void create(TapHandler on_tap, void* context);
    void show();
    void hide();

private:
    static void tapped_cb(lv_event_t* e);

    lv_obj_t* overlay = nullptr;
    MuseMascot mascot;
    TapHandler on_tap = nullptr;
    void* context = nullptr;
};
