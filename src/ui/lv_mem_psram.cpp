/*
 * LVGL allocator backed by PSRAM (LV_USE_STDLIB_MALLOC == LV_STDLIB_CUSTOM).
 *
 * With LV_STDLIB_CLIB, malloc() keeps small allocations in internal RAM, and
 * LVGL's widgets are all small — the UI consumed ~124KB of internal heap,
 * starving FreeRTOS task stacks at boot. Route LVGL to PSRAM explicitly and
 * fall back to internal RAM only if PSRAM is exhausted.
 */

#include <lvgl.h>

#if LV_USE_STDLIB_MALLOC == LV_STDLIB_CUSTOM

#include <esp_heap_caps.h>

static constexpr uint32_t kLvPsramCaps = MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT;

extern "C" {

void lv_mem_init(void) {}

void lv_mem_deinit(void) {}

lv_mem_pool_t lv_mem_add_pool(void* mem, size_t bytes) {
    LV_UNUSED(mem);
    LV_UNUSED(bytes);
    return NULL;
}

void lv_mem_remove_pool(lv_mem_pool_t pool) {
    LV_UNUSED(pool);
}

void* lv_malloc_core(size_t size) {
    void* p = heap_caps_malloc(size, kLvPsramCaps);
    return p ? p : heap_caps_malloc(size, MALLOC_CAP_8BIT);
}

void* lv_realloc_core(void* p, size_t new_size) {
    void* np = heap_caps_realloc(p, new_size, kLvPsramCaps);
    return np ? np : heap_caps_realloc(p, new_size, MALLOC_CAP_8BIT);
}

void lv_free_core(void* p) {
    heap_caps_free(p);
}

void lv_mem_monitor_core(lv_mem_monitor_t* mon_p) {
    LV_UNUSED(mon_p);
}

lv_result_t lv_mem_test_core(void) {
    return LV_RESULT_OK;
}

}  // extern "C"

#endif
