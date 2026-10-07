#pragma once
#include <stddef.h>

// Crash-surviving log for boards without a USB serial connection.
//
// Everything logged through LOG_BLE, BluetoothManager::log() and ESP-IDF's
// ESP_LOGx is mirrored into a 2KB ring buffer in RTC memory, which survives
// panics, watchdog resets and esp_restart() (not power loss). At boot the
// previous boot's buffer is kept together with the reset reason, and both are
// reported over BLE (`grinder.py info` / `grinder.py diagnostics`).
namespace CrashLog {

// Call first thing in setup(), before anything logs.
void init();

// printf to Serial and the ring buffer (what LOG_BLE expands to).
#if defined(__GNUC__)
void log_printf(const char* format, ...) __attribute__((format(printf, 1, 2)));
#else
void log_printf(const char* format, ...);
#endif

// Mirror already-printed text into the ring buffer.
void append(const char* text, size_t len);

// Why this boot happened, e.g. "PANIC", "TASK_WDT", "POWERON".
const char* reset_reason();

// Log captured before the last reset; empty after a power-on or first boot.
const char* previous_log();

// Copy this boot's log so far (oldest first, NUL-terminated) into out.
// Returns the number of characters copied.
size_t copy_current_log(char* out, size_t capacity);

}  // namespace CrashLog
