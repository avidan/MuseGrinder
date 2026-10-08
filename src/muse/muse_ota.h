#pragma once

class WebServer;
class GrindController;

// WiFi firmware updates: POST /ota (multipart, field "firmware") with header
// X-OTA-Password. The image streams straight into the spare app slot; it is
// only booted if it validates and its board-revision marker matches this
// board. Refused while a grind is running. Upload with tools/wifi_upload.py.
//
// The password is generated at build time into the gitignored .ota_password
// file (tools/build-scripts/pre_ota_secret.py) and compiled in as
// MUSE_OTA_PASSWORD. Without it the endpoint stays disabled.
void museOtaRegister(WebServer& server, GrindController* gc);
