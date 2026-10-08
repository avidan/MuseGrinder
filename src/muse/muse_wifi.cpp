/*
 * muse_wifi.cpp — Muse gadget add-on for the smart grind-by-weight firmware.
 *
 * Exposes the grinder to Muse over the LAN (Home Link):
 *   GET  /status  -> grinding state, live weight, target, last result
 *   POST /target  -> {"g": 18.5} starts a weight-based grind (1-100g)
 *   POST /dose    -> {"g": 18.5} sets the next dose (selected profile), no grind
 *   POST /stop    -> stops an active grind
 *   GET  /last    -> summary of the last completed grind
 *
 * First boot (or saved network unreachable for 20s after boot): join the
 * open "MuseGrinder-Setup" AP and enter your WiFi network at 192.168.4.1.
 * Afterwards: http://musegrinder.local
 *
 * WiFi never blocks boot — the grinder works normally while offline.
 *
 * Runs in the Arduino loop() task. GrindController serializes its entry
 * points with an internal mutex, so calls from here are safe alongside the
 * UI task and the grind control task.
 */

#include "muse_wifi.h"

#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <DNSServer.h>

#include "config/constants.h"
#include "controllers/grind_controller.h"
#include "controllers/profile_controller.h"
#include "controllers/grind_mode.h"
#include "hardware/WeightSensor.h"
#include "system/simulation_mode.h"
#include "system/board_id.h"
#include "system/crash_log.h"
#include "muse_ota.h"

#include <atomic>

static WebServer museServer(80);
static DNSServer museDns;
static bool s_mdnsStarted = false;
static bool s_apActive = false;
static bool s_everConnected = false;
static unsigned long s_wifiStartMs = 0;

static const char* MUSE_AP_SSID = "MuseGrinder-Setup";
static const unsigned long MUSE_AP_FALLBACK_MS = 20000;
static GrindController* s_gc = nullptr;
static WeightSensor* s_ws = nullptr;
static ProfileController* s_pc = nullptr;

// Next dose queued by POST /dose, in tenths of a gram; -1 when none pending.
static std::atomic<int> s_pendingDoseDg{-1};

static const float MUSE_DOSE_MIN_G = 5.0f;    // profile minimum
static const float MUSE_DOSE_MAX_G = 100.0f;  // same ceiling as /target

struct MuseLastGrind {
    float finalWeightG = 0;
    float targetG = 0;
    const char* result = "none";
    bool valid = false;
};
static MuseLastGrind s_last;
static bool s_prevFinished = false;

// is_active() stays true through COMPLETED/TIMEOUT until the screen is
// tapped (or auto-return fires), so "grinding" excludes those phases.
static bool museIsFinished() {
    return s_gc->is_finished();
}

static bool museIsGrinding() {
    return s_gc->is_active() && !museIsFinished();
}

static const char* museResultName(GrindController::GrindSessionResult r) {
    using R = GrindController::GrindSessionResult;
    switch (r) {
        case R::SUCCESS:    return "success";
        case R::OVERSHOOT:   return "overshoot";
        case R::MAX_PULSES:  return "max_pulses";
        case R::TIMEOUT:     return "timeout";
        case R::ERROR:       return "error";
        default:             return "unknown";
    }
}

// The dose the next button press will grind: a queued /dose if one hasn't
// been applied yet, otherwise the selected profile's weight.
static float museNextDose() {
    int pending = s_pendingDoseDg.load();
    return pending >= 0 ? pending / 10.0f : s_pc->get_current_weight();
}

// Parse {"g": <grams>} from the request body. Sends 400 and returns false
// if it's missing.
static bool museParseGrams(float* grams) {
    if (!museServer.hasArg("plain")) {
        museServer.send(400, "application/json", "{\"error\":\"missing body\"}");
        return false;
    }
    String body = museServer.arg("plain");
    int gPos = body.indexOf("\"g\"");
    if (gPos < 0) {
        museServer.send(400, "application/json", "{\"error\":\"expected {\\\"g\\\": <grams>}\"}");
        return false;
    }
    *grams = body.substring(body.indexOf(':', gPos) + 1).toFloat();
    return true;
}

static void museHandleStatus() {
    char buf[400];
    snprintf(buf, sizeof(buf),
        "{\"grinding\":%s,\"weight_g\":%.2f,\"target_g\":%.1f,"
        "\"next_dose_g\":%.1f,\"profile\":\"%s\","
        "\"mode\":\"weight\",\"last_result\":\"%s\","
        "\"simulated\":%s,\"build\":%d,\"board\":\"%s\",\"reset_reason\":\"%s\","
        "\"firmware\":\"musegrinder-muse/1.0\"}",
        museIsGrinding() ? "true" : "false",
        s_ws->get_display_weight(),
        s_gc->get_target_weight(),
        museNextDose(),
        s_pc->get_current_name(),
        museResultName(s_gc->get_last_session_result()),
        SimulationMode::enabled() ? "true" : "false",
        BUILD_NUMBER, BoardId::id(), CrashLog::reset_reason());
    museServer.send(200, "application/json", buf);
}

// Set the dose for the next grind (the selected profile) without grinding.
static void museHandleDose() {
    float g;
    if (!museParseGrams(&g)) return;
    if (g < MUSE_DOSE_MIN_G || g > MUSE_DOSE_MAX_G) {
        museServer.send(400, "application/json", "{\"error\":\"dose out of range (5-100g)\"}");
        return;
    }
    if (museIsGrinding()) {
        museServer.send(409, "application/json", "{\"error\":\"grind active\"}");
        return;
    }
    const int dg = static_cast<int>(g * 10.0f + 0.5f);
    s_pendingDoseDg.store(dg);

    char buf[128];
    snprintf(buf, sizeof(buf), "{\"next_dose_g\":%.1f,\"profile\":\"%s\",\"started\":false}",
             dg / 10.0f, s_pc->get_current_name());
    museServer.send(200, "application/json", buf);
}

bool museTakePendingDose(float* grams) {
    int dg = s_pendingDoseDg.exchange(-1);
    if (dg < 0) return false;
    *grams = dg / 10.0f;
    return true;
}

static void museHandleTarget() {
    float g;
    if (!museParseGrams(&g)) return;
    if (g < 1 || g > 100) {
        museServer.send(400, "application/json", "{\"error\":\"target out of range (1-100g)\"}");
        return;
    }
    if (museIsGrinding()) {
        museServer.send(409, "application/json", "{\"error\":\"grind already active\"}");
        return;
    }
    // Dismiss a finished grind first, same as the screen tap / auto-return.
    if (museIsFinished()) {
        s_gc->return_to_idle();
    }
    s_gc->start_grind(g, 0, GrindMode::WEIGHT);
    if (!s_gc->is_active()) {
        // start_grind refuses on a load cell fault or uncalibrated scale.
        museServer.send(503, "application/json",
            "{\"error\":\"grinder refused to start (load cell fault or not calibrated)\"}");
        return;
    }

    char buf[64];
    snprintf(buf, sizeof(buf), "{\"target_g\":%.1f,\"started\":true}", g);
    museServer.send(200, "application/json", buf);
}

static void museHandleStop() {
    if (!museIsGrinding()) {
        museServer.send(409, "application/json", "{\"error\":\"no grind active\"}");
        return;
    }
    s_gc->stop_grind();
    museServer.send(200, "application/json", "{\"stopped\":true}");
}

static void museHandleLast() {
    char buf[160];
    snprintf(buf, sizeof(buf),
        "{\"valid\":%s,\"final_weight_g\":%.2f,\"target_g\":%.1f,\"result\":\"%s\"}",
        s_last.valid ? "true" : "false",
        s_last.finalWeightG, s_last.targetG, s_last.result);
    museServer.send(200, "application/json", buf);
}

static void museHandleSetupPage() {
    if (!s_apActive) {
        museServer.send(200, "application/json", "{\"name\":\"musegrinder\"}");
        return;
    }
    museServer.send(200, "text/html",
        "<!doctype html><meta name=viewport content='width=device-width'>"
        "<title>MuseGrinder WiFi</title><h2>MuseGrinder WiFi</h2>"
        "<form method=post action=/wifi>"
        "<p><input name=ssid placeholder='Network name' required></p>"
        "<p><input name=pass type=password placeholder='Password'></p>"
        "<p><button>Connect</button></p></form>");
}

static void museHandleWifiSave() {
    if (!s_apActive) {
        museServer.send(403, "application/json", "{\"error\":\"setup mode not active\"}");
        return;
    }
    String ssid = museServer.arg("ssid");
    if (ssid.isEmpty()) {
        museServer.send(400, "text/plain", "Network name required");
        return;
    }
    museServer.send(200, "text/html",
        "<!doctype html><meta name=viewport content='width=device-width'>"
        "<p>Connecting&hellip; this network will close once the grinder joins your WiFi. "
        "Then use http://musegrinder.local</p>");
    // Credentials persist in NVS; WiFi.begin() with no args reuses them on boot.
    WiFi.begin(ssid.c_str(), museServer.arg("pass").c_str());
}

static void museStartSetupAp() {
    WiFi.mode(WIFI_AP_STA);
    WiFi.softAP(MUSE_AP_SSID);
    museDns.start(53, "*", WiFi.softAPIP());
    s_apActive = true;
    LOG_BLE("[MUSE] No WiFi after %lus - setup AP \"%s\" at %s\n",
            MUSE_AP_FALLBACK_MS / 1000, MUSE_AP_SSID, WiFi.softAPIP().toString().c_str());
}

static void museStopSetupAp() {
    museDns.stop();
    WiFi.softAPdisconnect(true);
    WiFi.mode(WIFI_STA);
    s_apActive = false;
}

void museWifiSetup(GrindController* gc, WeightSensor* ws, ProfileController* pc) {
    s_gc = gc;
    s_ws = ws;
    s_pc = pc;

    WiFi.mode(WIFI_STA);
    WiFi.setHostname("musegrinder");
    WiFi.begin(); // saved credentials, if any; connects in the background
    s_wifiStartMs = millis();

    museServer.on("/", HTTP_GET, museHandleSetupPage);
    museServer.on("/wifi", HTTP_POST, museHandleWifiSave);

    museServer.on("/status", HTTP_GET, museHandleStatus);
    museServer.on("/target", HTTP_POST, museHandleTarget);
    museServer.on("/stop", HTTP_POST, museHandleStop);
    museServer.on("/last", HTTP_GET, museHandleLast);
    museServer.on("/dose", HTTP_POST, museHandleDose);
    museOtaRegister(museServer, s_gc);
    museServer.onNotFound([]() {
        if (s_apActive) { // captive portal: send every unknown URL to the form
            museServer.sendHeader("Location", "http://192.168.4.1/");
            museServer.send(302, "text/plain", "");
            return;
        }
        museServer.send(404, "application/json", "{\"error\":\"not found\"}");
    });
    museServer.begin();
}

void museWifiLoop() {
    const bool connected = WiFi.status() == WL_CONNECTED;
    if (connected) {
        if (!s_everConnected) {
            LOG_BLE("[MUSE] WiFi connected: %s (http://musegrinder.local)\n",
                    WiFi.localIP().toString().c_str());
        }
        s_everConnected = true;
        if (s_apActive) museStopSetupAp();
        if (!s_mdnsStarted) s_mdnsStarted = MDNS.begin("musegrinder"); // http://musegrinder.local
    } else if (!s_everConnected && !s_apActive &&
               millis() - s_wifiStartMs > MUSE_AP_FALLBACK_MS) {
        // Only at boot; later drops rely on the driver's auto-reconnect.
        museStartSetupAp();
    }
    if (s_apActive) museDns.processNextRequest();

    museServer.handleClient();

    // Capture the last-grind summary when the grind finishes (settled weight,
    // cup still on the scale) rather than when the screen is dismissed.
    bool finished = museIsFinished();
    if (finished && !s_prevFinished) {
        s_last.targetG = s_gc->get_target_weight();
        s_last.finalWeightG = s_ws->get_display_weight();
        s_last.result = museResultName(s_gc->get_last_session_result());
        s_last.valid = true;
    }
    s_prevFinished = finished;
}
