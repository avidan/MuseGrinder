#pragma once
#include <stddef.h>
#include <esp_partition.h>

// Board revision marker embedded in every firmware image. V1 and V2 boards
// drive the motor relay from different GPIOs, so an image for the wrong
// revision would leave the relay input floating. Both update paths (BLE and
// WiFi) check the marker of a newly written image before booting it.
namespace BoardId {

enum class MarkerResult { MATCH, MISMATCH, MISSING };

// This build's board revision, e.g. "WS164-V1".
const char* id();

// Scan an app partition for the marker. found_id receives the image's board
// ID when one is present. Images without a marker (older builds) are MISSING.
MarkerResult check_partition(const esp_partition_t* part, char* found_id, size_t found_len);

}  // namespace BoardId
