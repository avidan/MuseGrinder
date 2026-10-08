#include "muse_mascot.h"
#include "jolly_anim.h" // generated at build time by tools/gen_jolly.py (gitignored)
#include <esp_heap_caps.h>

namespace {
constexpr int32_t kMaxFrameW = JOLLY_FRAME_W > JOLLY_STRAIN_W ? JOLLY_FRAME_W : JOLLY_STRAIN_W;
constexpr int32_t kMaxFrameH = JOLLY_FRAME_H > JOLLY_STRAIN_H ? JOLLY_FRAME_H : JOLLY_STRAIN_H;
}

void MuseMascot::create(lv_obj_t* parent, uint16_t frame_scale) {
    scale = frame_scale;
    const int32_t w = kMaxFrameW * scale / 256;
    const int32_t h = kMaxFrameH * scale / 256;

    // Frames sit bottom-centre in a box sized for the larger animation.
    root = lv_obj_create(parent);
    lv_obj_set_size(root, w, h);
    lv_obj_set_style_bg_opa(root, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(root, 0, 0);
    lv_obj_set_style_pad_all(root, 0, 0);
    lv_obj_clear_flag(root, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(root, LV_OBJ_FLAG_SCROLLABLE);

    img = lv_image_create(root);
    lv_obj_clear_flag(img, LV_OBJ_FLAG_CLICKABLE);

    if (scale != 256) {
        scaled_pixels = static_cast<uint16_t*>(
            heap_caps_malloc(static_cast<size_t>(w) * h * sizeof(uint16_t), MALLOC_CAP_SPIRAM));
    }
    if (scaled_pixels) {
        scaled_dsc.header.magic = LV_IMAGE_HEADER_MAGIC;
        scaled_dsc.header.cf = LV_COLOR_FORMAT_RGB565;
    } else if (scale != 256) {
        // No PSRAM buffer: fall back to LVGL scaling (slower to redraw).
        lv_image_set_inner_align(img, LV_IMAGE_ALIGN_STRETCH);
        lv_image_set_antialias(img, false);
    }
    show_frame(jolly_get_frame(0));

    // Shake, squash and effects are baked into the frames.
    lv_timer_create(timer_cb, 50, this);
}

void MuseMascot::setStraining(bool s) {
    if (straining == s) return;
    straining = s;
    // Swap the frame immediately (even while hidden) so a stale frame from
    // the other animation never flashes up.
    show_frame(straining ? jolly_get_strain_frame(0) : jolly_get_frame(0));
}

void MuseMascot::show_frame(const lv_image_dsc_t* frame) {
    if (!img || frame == shown_frame) return;
    shown_frame = frame;

    const int32_t sw = frame->header.w;
    const int32_t sh = frame->header.h;
    const int32_t w = sw * scale / 256;
    const int32_t h = sh * scale / 256;

    if (scaled_pixels) {
        // Nearest-neighbour upscale keeps the pixel art crisp.
        const uint16_t* src = reinterpret_cast<const uint16_t*>(frame->data);
        for (int32_t y = 0; y < h; y++) {
            const uint16_t* src_row = src + (y * sh / h) * sw;
            uint16_t* dst = scaled_pixels + y * w;
            for (int32_t x = 0; x < w; x++) {
                dst[x] = src_row[x * sw / w];
            }
        }
        scaled_dsc.header.w = w;
        scaled_dsc.header.h = h;
        scaled_dsc.header.stride = w * sizeof(uint16_t);
        scaled_dsc.data_size = static_cast<uint32_t>(w) * h * sizeof(uint16_t);
        scaled_dsc.data = reinterpret_cast<const uint8_t*>(scaled_pixels);
        lv_image_set_src(img, &scaled_dsc);
        lv_obj_invalidate(img);  // same descriptor, new pixels
    } else {
        lv_image_set_src(img, frame);
    }
    lv_obj_set_size(img, w, h);
    lv_obj_align(img, LV_ALIGN_BOTTOM_MID, 0, 0);
}

void MuseMascot::timer_cb(lv_timer_t* t) {
    auto* self = static_cast<MuseMascot*>(lv_timer_get_user_data(t));
    if (self) self->tick();
}

void MuseMascot::tick() {
    if (frozen || !img || !lv_obj_is_visible(img)) return;
    const uint32_t now = lv_tick_get();
    show_frame(straining ? jolly_get_strain_frame(now / JOLLY_STRAIN_FRAME_MS)
                         : jolly_get_frame(now / JOLLY_FRAME_MS));
}
