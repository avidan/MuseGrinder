#include "board_id.h"
#include "../config/hardware.h"
#include <Arduino.h>
#include <algorithm>
#include <ctype.h>
#include <string.h>

// Board revision marker embedded in every image. V1 and V2 boards drive the
// motor relay from different GPIOs, so an image for the wrong revision would
// leave the relay input floating. OTA refuses an image whose marker names a
// different revision.
#if HW_DISPLAY_VARIANT_V2
#define OTA_BOARD_ID "WS164-V2"
#else
#define OTA_BOARD_ID "WS164-V1"
#endif
#define OTA_BOARD_MARKER_PREFIX "\x01SGBW-BOARD:"
#define OTA_BOARD_MARKER_END '\x02'
extern "C" __attribute__((used)) const char kOtaBoardMarker[] =
    OTA_BOARD_MARKER_PREFIX OTA_BOARD_ID "\x02";

namespace BoardId {

const char* id() {
    // Read the ID out of the marker itself so the linker keeps the marker in
    // every image (gc-sections would drop an unreferenced array).
    static char buf[sizeof(OTA_BOARD_ID)] = {};
    if (!buf[0]) {
        memcpy(buf, kOtaBoardMarker + sizeof(OTA_BOARD_MARKER_PREFIX) - 1, sizeof(OTA_BOARD_ID) - 1);
    }
    return buf;
}

// Scan an app partition for the board marker. The bare prefix literal also
// appears in images (it is the needle below), so only a prefix followed by an
// ID and the end byte counts as a marker.
MarkerResult check_partition(const esp_partition_t* part, char* found_id, size_t found_len) {
    static const char prefix[] = OTA_BOARD_MARKER_PREFIX;
    const size_t prefix_len = sizeof(prefix) - 1;
    const size_t id_max = 16;
    const size_t chunk = 4096;
    const size_t overlap = prefix_len + id_max + 1;
    uint8_t* buf = static_cast<uint8_t*>(malloc(chunk + overlap));
    if (!buf) return MarkerResult::MISSING;

    MarkerResult result = MarkerResult::MISSING;
    size_t carried = 0;
    for (size_t offset = 0; offset < part->size && result == MarkerResult::MISSING; offset += chunk) {
        size_t n = std::min(chunk, static_cast<size_t>(part->size - offset));
        if (esp_partition_read(part, offset, buf + carried, n) != ESP_OK) break;
        size_t avail = carried + n;
        for (size_t i = 0; i + prefix_len < avail; i++) {
            if (buf[i] != static_cast<uint8_t>(prefix[0]) || memcmp(buf + i, prefix, prefix_len) != 0) continue;
            size_t j = i + prefix_len;
            size_t k = 0;
            while (j + k < avail && k < id_max && buf[j + k] != OTA_BOARD_MARKER_END && isprint(buf[j + k])) k++;
            if (j + k < avail && buf[j + k] == OTA_BOARD_MARKER_END && k > 0) {
                size_t copy = std::min(k, found_len - 1);
                memcpy(found_id, buf + j, copy);
                found_id[copy] = '\0';
                result = (k == strlen(OTA_BOARD_ID) && memcmp(buf + j, OTA_BOARD_ID, k) == 0)
                             ? MarkerResult::MATCH : MarkerResult::MISMATCH;
                break;
            }
        }
        carried = std::min(overlap, avail);
        memmove(buf, buf + avail - carried, carried);
    }
    free(buf);
    return result;
}

}  // namespace BoardId
