#include "muse_ota.h"

#include <Arduino.h>
#include <WebServer.h>
#include <esp_ota_ops.h>
#include <string.h>

#include "config/constants.h"
#include "controllers/grind_controller.h"
#include "system/board_id.h"
#include "muse_wifi.h"

#ifndef MUSE_OTA_PASSWORD
#define MUSE_OTA_PASSWORD ""
#endif

namespace {

WebServer* s_server = nullptr;
GrindController* s_gc = nullptr;

esp_ota_handle_t s_handle = 0;
const esp_partition_t* s_partition = nullptr;
bool s_writing = false;
bool s_success = false;
size_t s_written = 0;
int s_error_code = 0;
char s_error[96] = "";

void fail(int code, const char* message) {
    if (s_writing) {
        esp_ota_abort(s_handle);
        s_writing = false;
    }
    museWifiSetUploadActive(false);
    if (s_error_code == 0) {  // keep the first error
        s_error_code = code;
        strncpy(s_error, message, sizeof(s_error) - 1);
        LOG_BLE("[OTA-WIFI] Rejected: %s\n", message);
    }
}

// Constant-time comparison so response timing doesn't leak the password.
bool password_ok() {
    const char* expected = MUSE_OTA_PASSWORD;
    const String given = s_server->header("X-OTA-Password");
    const size_t len = strlen(expected);
    if (len == 0 || given.length() != len) return false;
    uint8_t diff = 0;
    for (size_t i = 0; i < len; i++) diff |= static_cast<uint8_t>(given[i] ^ expected[i]);
    return diff == 0;
}

void handle_upload() {
    HTTPUpload& upload = s_server->upload();

    switch (upload.status) {
        case UPLOAD_FILE_START:
            if (s_writing) {  // a previous upload stalled mid-transfer
                esp_ota_abort(s_handle);
                s_writing = false;
                LOG_BLE("[OTA-WIFI] Discarded an unfinished upload\n");
            }
            s_success = false;
            s_written = 0;
            s_error_code = 0;
            s_error[0] = '\0';
            if (strlen(MUSE_OTA_PASSWORD) == 0) {
                fail(403, "WiFi OTA disabled (no password compiled in)");
                return;
            }
            if (!password_ok()) {
                fail(401, "bad or missing X-OTA-Password");
                return;
            }
            if (s_gc && s_gc->is_active() && !s_gc->is_finished()) {
                fail(409, "grind active");
                return;
            }
            s_partition = esp_ota_get_next_update_partition(nullptr);
            if (!s_partition) {
                fail(500, "no OTA partition");
                return;
            }
            // Erase sector by sector as data arrives, not the whole slot up
            // front (that would block the HTTP loop for many seconds).
            if (esp_ota_begin(s_partition, OTA_WITH_SEQUENTIAL_WRITES, &s_handle) != ESP_OK) {
                fail(500, "esp_ota_begin failed");
                return;
            }
            s_writing = true;
            museWifiSetUploadActive(true);
            LOG_BLE("[OTA-WIFI] Receiving %s into %s\n", upload.filename.c_str(), s_partition->label);
            break;

        case UPLOAD_FILE_WRITE:
            if (!s_writing) return;
            if (esp_ota_write(s_handle, upload.buf, upload.currentSize) != ESP_OK) {
                fail(500, "flash write failed");
                return;
            }
            s_written += upload.currentSize;
            if ((s_written & 0x3FFFF) < upload.currentSize) {  // every ~256KB
                LOG_BLE("[OTA-WIFI] %u KB\n", (unsigned)(s_written / 1024));
            }
            break;

        case UPLOAD_FILE_END: {
            if (!s_writing) return;
            s_writing = false;
            museWifiSetUploadActive(false);
            if (esp_ota_end(s_handle) != ESP_OK) {  // validates the image
                fail(400, "image failed validation");
                return;
            }
            char found[24] = "";
            if (BoardId::check_partition(s_partition, found, sizeof(found)) != BoardId::MarkerResult::MATCH) {
                fail(400, found[0] ? "image is for a different board revision"
                                   : "image has no board marker");
                return;
            }
            if (esp_ota_set_boot_partition(s_partition) != ESP_OK) {
                fail(500, "could not set boot partition");
                return;
            }
            s_success = true;
            LOG_BLE("[OTA-WIFI] Update OK (%u KB) - restarting\n", (unsigned)(s_written / 1024));
            break;
        }

        case UPLOAD_FILE_ABORTED:
            fail(400, "upload aborted");
            break;
    }
}

void handle_done() {
    if (s_success) {
        s_server->send(200, "application/json", "{\"ok\":true,\"rebooting\":true}");
        delay(500);
        ESP.restart();
        return;
    }
    char body[160];
    snprintf(body, sizeof(body), "{\"ok\":false,\"error\":\"%s\"}",
             s_error[0] ? s_error : "no firmware received");
    s_server->send(s_error_code ? s_error_code : 400, "application/json", body);
}

}  // namespace

void museOtaRegister(WebServer& server, GrindController* gc) {
    s_server = &server;
    s_gc = gc;
    static const char* kHeaders[] = {"X-OTA-Password"};
    server.collectHeaders(kHeaders, 1);
    server.on("/ota", HTTP_POST, handle_done, handle_upload);
}
