// Simulator stand-in for src/muse/muse_mascot.cpp.
//
// The real player needs the Jolly frames that tools/gen_jolly.py generates at
// build time from Meta's artwork. Those are gitignored and never available in
// CI, so the simulator draws a placeholder of the same size instead. Layout and
// render-budget tests still see a mascot-sized object.
#include "muse/muse_mascot.h"

namespace {
constexpr int32_t kFrameW = 112;  // matches gen_jolly.py FRAME_W x FRAME_H
constexpr int32_t kFrameH = 124;
}

void MuseMascot::create(lv_obj_t* parent, uint16_t frame_scale) {
    scale = frame_scale;
    root = lv_obj_create(parent);
    lv_obj_set_size(root, kFrameW * scale / 256, kFrameH * scale / 256);
    lv_obj_set_style_bg_color(root, lv_color_hex(0x3A3A3A), 0);
    lv_obj_set_style_bg_opa(root, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(root, 0, 0);
    lv_obj_set_style_radius(root, 12, 0);
    lv_obj_clear_flag(root, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(root, LV_OBJ_FLAG_SCROLLABLE);

    img = lv_label_create(root);
    lv_label_set_text(img, "Jolly");
    lv_obj_set_style_text_color(img, lv_color_hex(0xFFFFFF), 0);
    lv_obj_center(img);
}

void MuseMascot::setStraining(bool s) {
    straining = s;
    if (img) lv_label_set_text(img, straining ? "Jolly (straining)" : "Jolly");
}
