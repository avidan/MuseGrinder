#pragma once
#include <lvgl.h>

// Jolly frame player for the grinding screen and the menu preview.
//
// Frames come from Meta's jollybot.gif, converted at build time by
// tools/gen_jolly.py into src/muse/jolly_anim.c/h — both gitignored and
// NEVER committed. The Jolly artwork is Meta-copyrighted (the gadget SDK's
// Apache license does not cover it); the generated frames are for personal
// use on the builder's own board only.
//
// Idle: the authentic happy bounce loop.
// Straining (while grinding): Jolly squeezing hard — eyes shut, clenched
// mouth, red face, sweat, strain vein, crouch-and-shake — the visual gag
// being that the real coffee grounds falling from the chute are the
// punchline. The mascot itself stays clean: no feces drawn, ever.
class MuseMascot {
public:
    // Creates the player as a child of parent. scale is LVGL fixed point
    // (256 = 1x, 512 = 2x); pixel art is drawn without smoothing.
    void create(lv_obj_t* parent, uint16_t scale = 256);
    // true while the grinder motor is running.
    void setStraining(bool straining);
    bool isStraining() const { return straining; }
    lv_obj_t* get_root() const { return root; }

private:
    static void timer_cb(lv_timer_t* t);
    void tick();
    void apply_size();  // idle and straining frames differ in size

    lv_obj_t* root = nullptr;
    lv_obj_t* img = nullptr;
    uint16_t scale = 256;
    bool straining = false;
};
