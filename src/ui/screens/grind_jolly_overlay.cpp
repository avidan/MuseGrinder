#include "grind_jolly_overlay.h"
#include "../../config/constants.h"

namespace {
// 3x: 336x372px. Wider than the 280px screen, so the edges of the shake
// clip slightly, but Jolly and the toilet fill most of the display.
constexpr uint16_t kJollyScale = 768;
}

void GrindJollyOverlay::create(TapHandler handler, void* ctx) {
    on_tap = handler;
    context = ctx;

    // Top layer, so it covers every screen without being part of one.
    overlay = lv_obj_create(lv_layer_top());
    lv_obj_set_size(overlay, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_color(overlay, lv_color_hex(THEME_COLOR_BACKGROUND), 0);
    lv_obj_set_style_bg_opa(overlay, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(overlay, 0, 0);
    lv_obj_set_style_radius(overlay, 0, 0);
    lv_obj_set_style_pad_all(overlay, 0, 0);
    lv_obj_clear_flag(overlay, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(overlay, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(overlay, tapped_cb, LV_EVENT_CLICKED, this);

    mascot.create(overlay, kJollyScale);
    mascot.setStraining(true);
    // Toilet on the bottom edge of the screen.
    lv_obj_align(mascot.get_root(), LV_ALIGN_BOTTOM_MID, 0, 0);

    hide();
}

void GrindJollyOverlay::show() {
    if (overlay) lv_obj_clear_flag(overlay, LV_OBJ_FLAG_HIDDEN);
}

void GrindJollyOverlay::hide() {
    if (overlay) lv_obj_add_flag(overlay, LV_OBJ_FLAG_HIDDEN);
}

void GrindJollyOverlay::tapped_cb(lv_event_t* e) {
    auto* self = static_cast<GrindJollyOverlay*>(lv_event_get_user_data(e));
    if (self && self->on_tap) self->on_tap(self->context);
}
