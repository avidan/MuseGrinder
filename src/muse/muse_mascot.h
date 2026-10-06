#pragma once
#include <lvgl.h>

// Jolly frame player for the grinding screen.
//
// Frames come from Meta's jollybot.gif, converted at build time by
// tools/gen_jolly.py into src/muse/jolly_anim.c/h — both gitignored and
// NEVER committed. The Jolly artwork is Meta-copyrighted (the gadget SDK's
// Apache license does not cover it); the generated frames are for personal
// use on the builder's own board only.
//
// Idle: the authentic happy bounce loop.
// Straining (while grinding): same frames, shaken side-to-side with a
// squash-and-stretch pulse plus motion lines — the visual gag being that the
// real coffee grounds falling from the chute are the punchline. The mascot
// itself stays clean: no feces drawn, ever.
class MuseMascot {
public:
    // Creates the player as a child of parent (expects a flex column).
    void create(lv_obj_t* parent);
    // true while the grinder motor is running.
    void setStraining(bool straining);

private:
    static void timer_cb(lv_timer_t* t);
    void tick();

    lv_obj_t* root = nullptr;
    lv_obj_t* img = nullptr;
    lv_obj_t* lines[6] = {nullptr, nullptr, nullptr, nullptr, nullptr, nullptr};
    bool straining = false;
    uint32_t frame = 0;
};
