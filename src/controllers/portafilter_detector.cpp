#include "portafilter_detector.h"

#include <Arduino.h>
#include <Preferences.h>
#include <math.h>

#include "../config/constants.h"
#include "../hardware/WeightSensor.h"
#include "../system/simulation_mode.h"

namespace {

constexpr uint32_t kUpdateIntervalMs = 100;
constexpr uint32_t kSteadyWindowMs = 800;   // weight must hold still this long
constexpr float kSteadyRangeG = 2.0f;       // ...within this range
constexpr float kNewPlateauG = 5.0f;        // steady value moved this much = new plateau
constexpr float kStepMinG = 250.0f;         // plausible portafilter insert/remove step
constexpr float kStepMaxG = 800.0f;
constexpr float kAbsentToleranceG = 40.0f;  // empty baseline +/- this = absent
constexpr float kBandMarginG = 40.0f;       // learned band +/- this = present (covers a dose)
constexpr float kNewPortafilterG = 80.0f;   // step this far outside the band = different portafilter
constexpr uint32_t kOkSteadyMs = 1000;      // present this long before a grind may start
constexpr float kEmptyDriftAlpha = 0.02f;   // slow tracking of empty-scale drift

constexpr const char* kPrefsNamespace = "portafilter";

}  // namespace

PortafilterDetector portafilter_detector;

void PortafilterDetector::init(WeightSensor* sensor) {
    sensor_ = sensor;
    load();
}

void PortafilterDetector::update() {
    const uint32_t now = millis();
    if (now - last_update_ms_ < kUpdateIntervalMs) return;
    last_update_ms_ = now;

    if (SimulationMode::enabled()) {
        // The simulated scale has no portafilter; never block simulated grinds.
        state_.store(State::UNKNOWN);
        return;
    }
    // Need a full steady window of samples (is_data_ready() only means "a new
    // HX711 sample is waiting", which the sampling task clears almost at once).
    if (!sensor_ || sensor_->has_hardware_fault() || sensor_->get_sample_count() < 16) {
        state_.store(State::UNKNOWN);
        return;
    }
    const float cal = sensor_->get_calibration_factor();
    if (fabsf(cal) < 1e-3f) {
        state_.store(State::UNKNOWN);
        return;
    }
    // Learned values are only valid for the calibration they were measured with.
    if (learned_cal_factor_ != 0.0f && fabsf(cal - learned_cal_factor_) > fabsf(learned_cal_factor_) * 0.01f) {
        LOG_BLE("[PORTAFILTER] Calibration changed - forgetting learned weights\n");
        have_empty_ = have_band_ = false;
        learned_cal_factor_ = 0.0f;
        save();
    }

    const float untared_g = static_cast<float>(sensor_->get_raw_adc_smoothed(300)) / cal;
    const bool steady = sensor_->get_weight_range(kSteadyWindowMs) < kSteadyRangeG;
    if (!steady) {
        if (state_.load() != State::MOVING) {
            state_.store(State::MOVING);
            steady_since_ms_.store(0);
        }
        return;
    }

    if (!have_plateau_ || fabsf(untared_g - plateau_g_) > kNewPlateauG) {
        if (have_plateau_) learn_from_step(plateau_g_, untared_g);
        plateau_g_ = untared_g;
        plateau_since_ms_ = now;
        have_plateau_ = true;
    }
    classify(untared_g, now);
}

void PortafilterDetector::learn_from_step(float before_g, float after_g) {
    const float step = after_g - before_g;
    const float magnitude = fabsf(step);
    if (magnitude < kStepMinG || magnitude > kStepMaxG) return;

    // Inserting: the scale was empty before. Removing: it's empty after.
    empty_g_ = step > 0 ? before_g : after_g;
    have_empty_ = true;
    observe_portafilter(magnitude);
    learned_cal_factor_ = sensor_->get_calibration_factor();
    save();
    LOG_BLE("[PORTAFILTER] %s %.0fg step - empty %.1fg, band %.0f-%.0fg\n",
            step > 0 ? "Inserted" : "Removed", magnitude, empty_g_, band_min_g_, band_max_g_);
}

void PortafilterDetector::observe_portafilter(float weight_g) {
    const bool outside = weight_g < band_min_g_ - kNewPortafilterG || weight_g > band_max_g_ + kNewPortafilterG;
    if (!have_band_ || outside) {
        band_min_g_ = band_max_g_ = weight_g;
        have_band_ = true;
        return;
    }
    band_min_g_ = fminf(band_min_g_, weight_g);
    band_max_g_ = fmaxf(band_max_g_, weight_g);
}

void PortafilterDetector::classify(float plateau_g, uint32_t now) {
    State next = State::UNKNOWN;
    if (have_empty_) {
        const float rel = plateau_g - empty_g_;
        if (fabsf(rel) < kAbsentToleranceG) {
            next = State::ABSENT;
            empty_g_ += (plateau_g - empty_g_) * kEmptyDriftAlpha;  // follow slow drift (not persisted)
        } else if (have_band_ && rel > band_min_g_ - kBandMarginG && rel < band_max_g_ + kBandMarginG) {
            next = State::PRESENT;
        } else {
            next = State::OTHER;
        }
    }
    if (state_.load() != next) {
        state_.store(next);
        steady_since_ms_.store(plateau_since_ms_ ? plateau_since_ms_ : now);
    }
}

bool PortafilterDetector::ok_to_grind() const {
    const State s = state_.load();
    if (s == State::UNKNOWN) return true;
    if (s != State::PRESENT) return false;
    const uint32_t since = steady_since_ms_.load();
    return since != 0 && millis() - since >= kOkSteadyMs;
}

const char* PortafilterDetector::state_name(State state) {
    switch (state) {
        case State::ABSENT: return "absent";
        case State::PRESENT: return "present";
        case State::MOVING: return "moving";
        case State::OTHER: return "other";
        default: return "unknown";
    }
}

const char* PortafilterDetector::blocked_reason() const {
    switch (state_.load()) {
        case State::ABSENT: return "Insert portafilter";
        case State::MOVING: return "Settling...";
        case State::OTHER: return "Check portafilter";
        default: return "Settling...";  // present but not steady for long enough yet
    }
}

void PortafilterDetector::load() {
    Preferences prefs;
    if (!prefs.begin(kPrefsNamespace, true)) return;  // nothing learned yet
    learned_cal_factor_ = prefs.getFloat("cal", 0.0f);
    have_empty_ = prefs.isKey("empty");
    empty_g_ = prefs.getFloat("empty", 0.0f);
    have_band_ = prefs.isKey("band_min");
    band_min_g_ = prefs.getFloat("band_min", 0.0f);
    band_max_g_ = prefs.getFloat("band_max", 0.0f);
    prefs.end();
    if (have_band_) {
        LOG_BLE("[PORTAFILTER] Learned: empty %.1fg, band %.0f-%.0fg\n", empty_g_, band_min_g_, band_max_g_);
    }
}

void PortafilterDetector::save() const {
    Preferences prefs;
    if (!prefs.begin(kPrefsNamespace, false)) return;
    if (have_empty_ && have_band_) {
        prefs.putFloat("cal", learned_cal_factor_);
        prefs.putFloat("empty", empty_g_);
        prefs.putFloat("band_min", band_min_g_);
        prefs.putFloat("band_max", band_max_g_);
    } else {
        prefs.clear();
    }
    prefs.end();
}
