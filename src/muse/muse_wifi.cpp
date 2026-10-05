/*
 * muse_wifi.cpp — Muse gadget add-on for the smart grind-by-weight firmware.
 *
 * Exposes the grinder to Muse over the LAN (Home Link):
 *   GET  /status  -> grinding state, live weight, target, last result
 *   POST /target  -> {"g": 18.5} starts a weight-based grind (1-100g)
 *   POST /stop    -> stops an active grind
 *   GET  /last    -> summary of the last completed grind
 *
 * First boot: join the "MuseGrinder-Setup" AP and pick your WiFi network.
 * Afterwards: http://musegrinder.local
 *
 * Calls GrindController from the Core 1 loop task — the same context the
 * touch UI uses, so no new threading concerns.
 */

#include "muse_wifi.h"

#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <WiFiManager.h>

#include "controllers/grind_controller.h"
#include "controllers/grind_mode.h"
#include "hardware/WeightSensor.h"

static WebServer museServer(80);
static GrindController* s_gc = nullptr;
static WeightSensor* s_ws = nullptr;

struct MuseLastGrind {
    float finalWeightG = 0;
    float targetG = 0;
    const char* result = "none";
    bool valid = false;
};
static MuseLastGrind s_last;
static bool s_prevActive = false;

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

static void museHandleStatus() {
    char buf[256];
    snprintf(buf, sizeof(buf),
        "{\"grinding\":%s,\"weight_g\":%.2f,\"target_g\":%.1f,"
        "\"mode\":\"weight\",\"last_result\":\"%s\","
        "\"firmware\":\"musegrinder-muse/1.0\"}",
        s_gc->is_active() ? "true" : "false",
        s_ws->get_display_weight(),
        s_gc->get_target_weight(),
        museResultName(s_gc->get_last_session_result()));
    museServer.send(200, "application/json", buf);
}

static void museHandleTarget() {
    if (!museServer.hasArg("plain")) {
        museServer.send(400, "application/json", "{\"error\":\"missing body\"}");
        return;
    }
    String body = museServer.arg("plain");
    int gPos = body.indexOf("\"g\"");
    if (gPos < 0) {
        museServer.send(400, "application/json", "{\"error\":\"expected {\\\"g\\\": <grams>}\"}");
        return;
    }
    float g = body.substring(body.indexOf(':', gPos) + 1).toFloat();
    if (g < 1 || g > 100) {
        museServer.send(400, "application/json", "{\"error\":\"target out of range (1-100g)\"}");
        return;
    }
    if (s_gc->is_active()) {
        museServer.send(409, "application/json", "{\"error\":\"grind already active\"}");
        return;
    }
    s_gc->start_grind(g, 0, GrindMode::WEIGHT);

    char buf[64];
    snprintf(buf, sizeof(buf), "{\"target_g\":%.1f,\"started\":true}", g);
    museServer.send(200, "application/json", buf);
}

static void museHandleStop() {
    if (!s_gc->is_active()) {
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

void museWifiSetup(GrindController* gc, WeightSensor* ws) {
    s_gc = gc;
    s_ws = ws;

    WiFiManager wm;
    wm.setConnectTimeout(20);
    if (!wm.autoConnect("MuseGrinder-Setup")) {
        ESP.restart();
    }

    if (MDNS.begin("musegrinder")) {
        // http://musegrinder.local
    }

    museServer.on("/status", HTTP_GET, museHandleStatus);
    museServer.on("/target", HTTP_POST, museHandleTarget);
    museServer.on("/stop", HTTP_POST, museHandleStop);
    museServer.on("/last", HTTP_GET, museHandleLast);
    museServer.onNotFound([]() {
        museServer.send(404, "application/json", "{\"error\":\"not found\"}");
    });
    museServer.begin();
}

void museWifiLoop() {
    museServer.handleClient();

    // Capture the last-grind summary on the active -> idle edge.
    bool active = s_gc->is_active();
    if (s_prevActive && !active) {
        s_last.targetG = s_gc->get_target_weight();
        s_last.finalWeightG = s_ws->get_display_weight();
        s_last.result = museResultName(s_gc->get_last_session_result());
        s_last.valid = true;
    }
    s_prevActive = active;
}
