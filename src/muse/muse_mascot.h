#pragma once
#include <lvgl.h>

// Animated Muse mascot for the grinding screen.
//
// Idle: happy face, gentle bob, occasional blink.
// Straining (while grinding): eyes squeezed shut (><), "o" mouth, blush
// cheeks, motion lines, and a full-body shudder — the visual gag being that
// the real coffee grounds falling from the chute are the punchline. The
// mascot itself stays clean: no feces drawn, ever.
//
// Built entirely from LVGL primitives (no image assets). Animation runs on
// an lv_timer at 10fps; tick() early-outs while the screen is hidden.
class MuseMascot {
public:
    // Creates the mascot as a child of parent (expects a flex column).
    void create(lv_obj_t* parent);
    // true while the grinder motor is running.
    void setStraining(bool straining);

private:
    static void timer_cb(lv_timer_t* t);
    void tick();
    void applyState();

    lv_obj_t* root = nullptr;   // flex item (static)
    lv_obj_t* body = nullptr;   // inner container (animated x/y)
    lv_obj_t* eye_l = nullptr;
    lv_obj_t* eye_r = nullptr;
    lv_obj_t* pupil_l = nullptr;
    lv_obj_t* pupil_r = nullptr;
    lv_obj_t* squint_l = nullptr; // ">" label
    lv_obj_t* squint_r = nullptr; // "<" label
    lv_obj_t* smile = nullptr;    // arc
    lv_obj_t* o_mouth = nullptr;  // "o" label
    lv_obj_t* cheek_l = nullptr;
    lv_obj_t* cheek_r = nullptr;
    lv_obj_t* lines[6] = {nullptr, nullptr, nullptr, nullptr, nullptr, nullptr};
    bool straining = false;
    uint32_t frame = 0;
};
