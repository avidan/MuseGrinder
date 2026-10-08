#include "crash_log.h"

#include <Arduino.h>
#include <esp_attr.h>
#include <esp_log.h>
#include <esp_system.h>
#include <freertos/FreeRTOS.h>
#include <stdarg.h>
#include <string.h>

namespace {

constexpr uint32_t kMagic = 0x43524C47;  // "CRLG"
constexpr size_t kRingSize = 2048;

struct RingBuffer {
    uint32_t magic;
    uint32_t head;     // next write position
    uint32_t wrapped;  // 1 once the buffer has filled up
    char data[kRingSize];
};

// What the panic handler saw (the ring can't log the crash itself).
constexpr uint32_t kPanicMagic = 0x50414E43;  // "PANC"
constexpr int kBacktraceDepth = 10;
struct PanicRecord {
    uint32_t magic;
    int32_t core;
    char reason[48];
    uint32_t pc;
    uint32_t backtrace[kBacktraceDepth];
};

// RTC memory that the bootloader leaves alone across resets.
RTC_NOINIT_ATTR RingBuffer s_ring;
RTC_NOINIT_ATTR PanicRecord s_panic;

char s_previous[kRingSize + 512];
esp_reset_reason_t s_reason = ESP_RST_UNKNOWN;
portMUX_TYPE s_mux = portMUX_INITIALIZER_UNLOCKED;
vprintf_like_t s_idf_vprintf = nullptr;
bool s_ready = false;

void append_locked(const char* text, size_t len) {
    for (size_t i = 0; i < len; i++) {
        s_ring.data[s_ring.head] = text[i];
        s_ring.head = (s_ring.head + 1) % kRingSize;
        if (s_ring.head == 0) s_ring.wrapped = 1;
    }
}

// Called by the Arduino core's panic wrapper with a decoded backtrace, before
// ESP-IDF prints the crash and resets. Record it for the next boot.
void on_panic(arduino_panic_info_t* info, void*) {
    memset(&s_panic, 0, sizeof(s_panic));
    s_panic.core = info->core;
    if (info->reason) strncpy(s_panic.reason, info->reason, sizeof(s_panic.reason) - 1);
    s_panic.pc = reinterpret_cast<uint32_t>(info->pc);
    for (int i = 0; i < kBacktraceDepth && i < info->backtrace_len; i++) {
        s_panic.backtrace[i] = info->backtrace[i];
    }
    s_panic.magic = kPanicMagic;
}

// ESP-IDF logs (ESP_LOGx) go through here too.
int idf_vprintf_hook(const char* format, va_list args) {
    char buf[200];
    va_list copy;
    va_copy(copy, args);
    int n = vsnprintf(buf, sizeof(buf), format, copy);
    va_end(copy);
    if (n > 0) CrashLog::append(buf, n < (int)sizeof(buf) ? n : sizeof(buf) - 1);
    return s_idf_vprintf ? s_idf_vprintf(format, args) : n;
}

}  // namespace

namespace CrashLog {

void init() {
    s_reason = esp_reset_reason();
    s_previous[0] = '\0';

    // RTC memory is undefined after power-on; trust it only with the magic.
    const bool warm = s_reason != ESP_RST_POWERON && s_reason != ESP_RST_BROWNOUT;
    const bool valid = warm && s_ring.magic == kMagic && s_ring.head < kRingSize;
    if (valid) {
        size_t len = 0;
        if (s_ring.wrapped) {
            const size_t tail = kRingSize - s_ring.head;
            memcpy(s_previous, s_ring.data + s_ring.head, tail);
            len = tail;
        }
        memcpy(s_previous + len, s_ring.data, s_ring.head);
        len += s_ring.head;
        s_previous[len] = '\0';
    }

    // Append what the panic handler recorded, for addr2line against firmware.elf.
    if (warm && s_panic.magic == kPanicMagic) {
        size_t len = strlen(s_previous);
        s_panic.reason[sizeof(s_panic.reason) - 1] = '\0';
        len += snprintf(s_previous + len, sizeof(s_previous) - len,
                        "\n*** PANIC on core %ld: %s\n    PC 0x%08lx\n    Backtrace:",
                        (long)s_panic.core, s_panic.reason, (unsigned long)s_panic.pc);
        for (int i = 0; i < kBacktraceDepth && s_panic.backtrace[i] && len < sizeof(s_previous); i++) {
            len += snprintf(s_previous + len, sizeof(s_previous) - len, " 0x%08lx",
                            (unsigned long)s_panic.backtrace[i]);
        }
        if (len < sizeof(s_previous)) snprintf(s_previous + len, sizeof(s_previous) - len, "\n");
    }
    s_panic.magic = 0;

    s_ring.magic = kMagic;
    s_ring.head = 0;
    s_ring.wrapped = 0;
    s_ready = true;

    s_idf_vprintf = esp_log_set_vprintf(idf_vprintf_hook);
    set_arduino_panic_handler(on_panic, nullptr);
}

void append(const char* text, size_t len) {
    if (!s_ready || !text || len == 0) return;
    portENTER_CRITICAL(&s_mux);
    append_locked(text, len);
    portEXIT_CRITICAL(&s_mux);
}

void log_printf(const char* format, ...) {
    char buf[200];
    va_list args;
    va_start(args, format);
    int n = vsnprintf(buf, sizeof(buf), format, args);
    va_end(args);
    if (n < 0) return;

    if (n < (int)sizeof(buf)) {
        Serial.write(reinterpret_cast<const uint8_t*>(buf), n);
        append(buf, n);
        return;
    }

    // Long line: format it fully for Serial; the ring keeps the start.
    char* big = static_cast<char*>(malloc(n + 1));
    if (!big) {
        Serial.write(reinterpret_cast<const uint8_t*>(buf), sizeof(buf) - 1);
        append(buf, sizeof(buf) - 1);
        return;
    }
    va_start(args, format);
    vsnprintf(big, n + 1, format, args);
    va_end(args);
    Serial.write(reinterpret_cast<const uint8_t*>(big), n);
    append(big, n);
    free(big);
}

const char* reset_reason() {
    switch (s_reason) {
        case ESP_RST_POWERON: return "POWERON";
        case ESP_RST_EXT: return "EXT";
        case ESP_RST_SW: return "SW";
        case ESP_RST_PANIC: return "PANIC";
        case ESP_RST_INT_WDT: return "INT_WDT";
        case ESP_RST_TASK_WDT: return "TASK_WDT";
        case ESP_RST_WDT: return "WDT";
        case ESP_RST_DEEPSLEEP: return "DEEPSLEEP";
        case ESP_RST_BROWNOUT: return "BROWNOUT";
        case ESP_RST_SDIO: return "SDIO";
        default: return "UNKNOWN";
    }
}

const char* previous_log() {
    return s_previous;
}

size_t copy_current_log(char* out, size_t capacity) {
    if (!out || capacity == 0) return 0;
    size_t len = 0;
    portENTER_CRITICAL(&s_mux);
    const size_t total = s_ring.wrapped ? kRingSize : s_ring.head;
    const size_t start = s_ring.wrapped ? s_ring.head : 0;
    for (size_t i = 0; i < total && len + 1 < capacity; i++) {
        out[len++] = s_ring.data[(start + i) % kRingSize];
    }
    portEXIT_CRITICAL(&s_mux);
    out[len] = '\0';
    return len;
}

}  // namespace CrashLog

