#include "muse_mascot.h"

namespace {

const lv_color_t INK = lv_color_hex(0x2A2A3C);
const lv_color_t HEAD = lv_color_hex(0x8F9CF7);   // periwinkle, Muse-adjacent
const lv_color_t BLUSH = lv_color_hex(0xF0A0B4);
const lv_color_t SPEED = lv_color_hex(0xC9D2FF);

lv_obj_t* circle(lv_obj_t* parent, int x, int y, int d, lv_color_t c, lv_opa_t opa = LV_OPA_COVER) {
    lv_obj_t* o = lv_obj_create(parent);
    lv_obj_set_size(o, d, d);
    lv_obj_set_pos(o, x, y);
    lv_obj_set_style_radius(o, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(o, c, 0);
    lv_obj_set_style_bg_opa(o, opa, 0);
    lv_obj_set_style_border_width(o, 0, 0);
    lv_obj_set_style_pad_all(o, 0, 0);
    lv_obj_clear_flag(o, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(o, LV_OBJ_FLAG_SCROLLABLE);
    return o;
}

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
    lv_obj_set_size(root, 120, 112);
    transparent(root);

    // Inner body: this is what shakes. Root stays put for the flex layout.
    body = lv_obj_create(root);
    lv_obj_set_size(body, 120, 112);
    lv_obj_set_pos(body, 0, 0);
    transparent(body);

    // Head
    circle(body, 16, 8, 88, HEAD);
    circle(body, 34, 22, 18, lv_color_hex(0xFFFFFF), LV_OPA_40); // gloss

    // Eyes (white + pupil). Pupils are children so blinking squashes both.
    eye_l = circle(body, 32, 38, 24, lv_color_hex(0xFFFFFF));
    eye_r = circle(body, 64, 38, 24, lv_color_hex(0xFFFFFF));
    pupil_l = circle(eye_l, 0, 0, 11, INK);
    lv_obj_center(pupil_l);
    pupil_r = circle(eye_r, 0, 0, 11, INK);
    lv_obj_center(pupil_r);

    // Squeezed-shut eyes for straining: "><"
    squint_l = lv_label_create(body);
    lv_label_set_text(squint_l, ">");
    lv_obj_set_style_text_font(squint_l, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(squint_l, INK, 0);
    lv_obj_set_pos(squint_l, 36, 40);
    lv_obj_add_flag(squint_l, LV_OBJ_FLAG_HIDDEN);

    squint_r = lv_label_create(body);
    lv_label_set_text(squint_r, "<");
    lv_obj_set_style_text_font(squint_r, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(squint_r, INK, 0);
    lv_obj_set_pos(squint_r, 68, 40);
    lv_obj_add_flag(squint_r, LV_OBJ_FLAG_HIDDEN);

    // Smile (idle)
    smile = lv_arc_create(body);
    lv_obj_set_size(smile, 44, 44);
    lv_obj_set_pos(smile, 38, 44);
    lv_arc_set_bg_angles(smile, 30, 150);
    lv_obj_set_style_arc_color(smile, INK, LV_PART_MAIN);
    lv_obj_set_style_arc_width(smile, 4, LV_PART_MAIN);
    lv_obj_set_style_arc_width(smile, 0, LV_PART_INDICATOR);
    lv_obj_remove_style(smile, nullptr, LV_PART_KNOB);
    lv_obj_clear_flag(smile, LV_OBJ_FLAG_CLICKABLE);

    // "o" mouth (straining)
    o_mouth = lv_label_create(body);
    lv_label_set_text(o_mouth, "o");
    lv_obj_set_style_text_font(o_mouth, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(o_mouth, INK, 0);
    lv_obj_set_pos(o_mouth, 53, 58);
    lv_obj_add_flag(o_mouth, LV_OBJ_FLAG_HIDDEN);

    // Effort blush
    cheek_l = circle(body, 22, 60, 16, BLUSH);
    lv_obj_add_flag(cheek_l, LV_OBJ_FLAG_HIDDEN);
    cheek_r = circle(body, 82, 60, 16, BLUSH);
    lv_obj_add_flag(cheek_r, LV_OBJ_FLAG_HIDDEN);

    // Motion lines
    const int ys[3] = {34, 52, 70};
    for (int i = 0; i < 3; i++) {
        lines[i] = hbar(body, 2, ys[i], 12, SPEED);
        lv_obj_add_flag(lines[i], LV_OBJ_FLAG_HIDDEN);
        lines[i + 3] = hbar(body, 106, ys[i], 12, SPEED);
        lv_obj_add_flag(lines[i + 3], LV_OBJ_FLAG_HIDDEN);
    }

    lv_timer_create(timer_cb, 100, this);
    applyState();
}

void MuseMascot::setStraining(bool s) {
    if (straining == s) return;
    straining = s;
    applyState();
}

void MuseMascot::applyState() {
    set_shown(eye_l, !straining);
    set_shown(eye_r, !straining);
    set_shown(squint_l, straining);
    set_shown(squint_r, straining);
    set_shown(smile, !straining);
    set_shown(o_mouth, straining);
    set_shown(cheek_l, straining);
    set_shown(cheek_r, straining);
    for (auto l : lines) set_shown(l, straining);
    if (!straining && body) {
        lv_obj_set_x(body, 0);
        lv_obj_set_y(body, 0);
        lv_obj_set_height(eye_l, 24);
        lv_obj_set_height(eye_r, 24);
    }
}

void MuseMascot::timer_cb(lv_timer_t* t) {
    auto* self = static_cast<MuseMascot*>(lv_timer_get_user_data(t));
    if (self) self->tick();
}

void MuseMascot::tick() {
    if (!body || !lv_obj_is_visible(body)) return;
    frame++;
    if (straining) {
        // The shudder: rapid side-to-side shake, slight vertical bounce.
        lv_obj_set_x(body, (frame & 1) ? 5 : -5);
        lv_obj_set_y(body, ((frame & 3) < 2) ? 2 : -2);
        // Motion lines flicker.
        lv_opa_t o = ((frame & 3) < 2) ? LV_OPA_COVER : LV_OPA_30;
        for (auto l : lines) lv_obj_set_style_opa(l, o, 0);
    } else {
        // Idle: gentle bob + blink every ~3.2s.
        lv_obj_set_x(body, 0);
        lv_obj_set_y(body, (((frame >> 3) & 1) ? 3 : -3));
        bool blink = (frame % 32) < 2;
        lv_obj_set_height(eye_l, blink ? 5 : 24);
        lv_obj_set_height(eye_r, blink ? 5 : 24);
    }
}
