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
    // (256 = 1x, 512 = 2x). Scaled frames are pre-rendered (nearest
    // neighbour) into a PSRAM buffer once per frame change, so LVGL only
    // blits them — no per-refresh transform.
    void create(lv_obj_t* parent, uint16_t scale = 256);
    // true while the grinder motor is running.
    void setStraining(bool straining);
    bool isStraining() const { return straining; }
    // Hold the current frame (e.g. while the motor is stopped).
    void setFrozen(bool frozen_) { frozen = frozen_; }
    lv_obj_t* get_root() const { return root; }

private:
    static void timer_cb(lv_timer_t* t);
    void tick();
    void show_frame(const lv_image_dsc_t* frame);

    lv_obj_t* root = nullptr;
    lv_obj_t* img = nullptr;
    uint16_t scale = 256;
    bool straining = false;
    bool frozen = false;
    const lv_image_dsc_t* shown_frame = nullptr;
    uint16_t* scaled_pixels = nullptr;  // PSRAM, only when scale != 256
    lv_image_dsc_t scaled_dsc = {};
};
