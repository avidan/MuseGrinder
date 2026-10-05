#include "muse_mascot.h"
#include "jolly_anim.h" // generated at build time by tools/gen_jolly.py (gitignored)

namespace {

// Base position of the 112px image inside the 160px root (centered).
constexpr int IMG_X0 = (160 - JOLLY_FRAME_W) / 2; // 24
constexpr int IMG_Y0 = (120 - JOLLY_FRAME_H) / 2; // 4

lv_obj_t* hbar(lv_obj_t* parent, int x, int y, int w, lv_color_t c) {
    lv_obj_t* o = lv_obj_create(parent);
    lv_obj_set_size(o, w, 4);
    lv_obj_set_pos(o, x, y);
    lv_obj_set_style_radius(o, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(o, c, 0);
    lv_obj_set_style_bg_opa(o, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(o, 0, 0);
    lv_obj_clear_flag(o, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(o, LV_OBJ_FLAG_SCROLLABLE);
    return o;
}

void transparent(lv_obj_t* o) {
    lv_obj_set_style_bg_opa(o, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(o, 0, 0);
    lv_obj_set_style_pad_all(o, 0, 0);
    lv_obj_clear_flag(o, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(o, LV_OBJ_FLAG_SCROLLABLE);
}

void set_shown(lv_obj_t* o, bool shown) {
    if (shown) lv_obj_clear_flag(o, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_add_flag(o, LV_OBJ_FLAG_HIDDEN);
}

} // namespace

void MuseMascot::create(lv_obj_t* parent) {
    root = lv_obj_create(parent);
    lv_obj_set_size(root, 160, 120);
    transparent(root);

    img = lv_image_create(root);
    lv_obj_set_pos(img, IMG_X0, IMG_Y0);
    lv_obj_set_size(img, JOLLY_FRAME_W, JOLLY_FRAME_H);
    lv_image_set_src(img, jolly_get_frame(0));
    lv_obj_clear_flag(img, LV_OBJ_FLAG_CLICKABLE);
    // Squash pivots around the feet so straining reads as pressing down.
    lv_obj_set_style_transform_pivot_x(img, JOLLY_FRAME_W / 2, 0);
    lv_obj_set_style_transform_pivot_y(img, JOLLY_FRAME_H, 0);

    // Motion lines, shown while straining.
    const lv_color_t c = lv_color_hex(0xB9C2E8);
    const int ys[3] = {30, 55, 80};
    for (int i = 0; i < 3; i++) {
        lines[i] = hbar(root, 6, ys[i], 12, c);
        lv_obj_add_flag(lines[i], LV_OBJ_FLAG_HIDDEN);
        lines[i + 3] = hbar(root, 142, ys[i], 12, c);
        lv_obj_add_flag(lines[i + 3], LV_OBJ_FLAG_HIDDEN);
    }

    lv_timer_create(timer_cb, 100, this);
}

void MuseMascot::setStraining(bool s) {
    if (straining == s) return;
    straining = s;
    for (auto l : lines) set_shown(l, straining);
    if (!straining) {
        lv_obj_set_x(img, IMG_X0);
        lv_obj_set_y(img, IMG_Y0);
        lv_obj_set_style_transform_scale_x(img, 256, 0);
        lv_obj_set_style_transform_scale_y(img, 256, 0);
    }
}

void MuseMascot::timer_cb(lv_timer_t* t) {
    auto* self = static_cast<MuseMascot*>(lv_timer_get_user_data(t));
    if (self) self->tick();
}

void MuseMascot::tick() {
    if (!img || !lv_obj_is_visible(img)) return;
    frame++;
    lv_image_set_src(img, jolly_get_frame(frame % JOLLY_FRAME_COUNT));
    if (straining) {
        // Shudder + squash-and-stretch: the effort read.
        lv_obj_set_x(img, IMG_X0 + ((frame & 1) ? 5 : -5));
        int squash = ((frame & 3) < 2) ? 228 : 256;
        lv_obj_set_style_transform_scale_y(img, squash, 0);
        lv_opa_t o = ((frame & 3) < 2) ? LV_OPA_COVER : LV_OPA_30;
        for (auto l : lines) lv_obj_set_style_opa(l, o, 0);
    } else {
        lv_obj_set_x(img, IMG_X0);
        lv_obj_set_y(img, IMG_Y0);
    }
}
