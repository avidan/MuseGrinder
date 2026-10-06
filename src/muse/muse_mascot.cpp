#include "muse_mascot.h"
#include "jolly_anim.h" // generated at build time by tools/gen_jolly.py (gitignored)

void MuseMascot::create(lv_obj_t* parent, uint16_t scale) {
    const int32_t w = JOLLY_FRAME_W * scale / 256;
    const int32_t h = JOLLY_FRAME_H * scale / 256;

    root = lv_obj_create(parent);
    lv_obj_set_size(root, w + 8, h + 8);
    lv_obj_set_style_bg_opa(root, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(root, 0, 0);
    lv_obj_set_style_pad_all(root, 0, 0);
    lv_obj_clear_flag(root, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(root, LV_OBJ_FLAG_SCROLLABLE);

    img = lv_image_create(root);
    lv_image_set_src(img, jolly_get_frame(0));
    lv_obj_set_size(img, w, h);
    if (scale != 256) {
        lv_image_set_inner_align(img, LV_IMAGE_ALIGN_STRETCH);
        lv_image_set_antialias(img, false);
    }
    lv_obj_center(img);
    lv_obj_clear_flag(img, LV_OBJ_FLAG_CLICKABLE);

    // Shake, squash and effects are baked into the frames.
    lv_timer_create(timer_cb, 50, this);
}

void MuseMascot::setStraining(bool s) {
    straining = s;
}

void MuseMascot::timer_cb(lv_timer_t* t) {
    auto* self = static_cast<MuseMascot*>(lv_timer_get_user_data(t));
    if (self) self->tick();
}

void MuseMascot::tick() {
    if (!img || !lv_obj_is_visible(img)) return;
    const uint32_t now = lv_tick_get();
    const lv_image_dsc_t* next = straining
        ? jolly_get_strain_frame(now / JOLLY_STRAIN_FRAME_MS)
        : jolly_get_frame(now / JOLLY_FRAME_MS);
    if (lv_image_get_src(img) != next) {
        lv_image_set_src(img, next);
    }
}
