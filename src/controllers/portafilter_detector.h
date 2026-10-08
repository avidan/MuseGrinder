#pragma once
#include <atomic>
#include <stdint.h>

class WeightSensor;

// Detects whether the portafilter is in the holder, from the untared scale
// weight (tares reset at every grind, so tared weights can't tell).
//
// It learns from real insert/remove steps: a steady-to-steady change of
// 250-800g is the portafilter going in or out. The lower plateau becomes the
// empty-scale baseline and the step joins the portafilter's weight band (a
// portafilter reads differently depending on how it sits in the holder - e.g.
// 384g vs 433g). Learned values persist in NVS with the calibration factor
// they were measured under.
//
// Until something has been learned the state is UNKNOWN, which never blocks.
class PortafilterDetector {
public:
    enum class State : uint8_t { UNKNOWN, ABSENT, PRESENT, MOVING, OTHER };

    void init(WeightSensor* sensor);
    // Call regularly (throttled internally to 10 Hz). Same task as readers of
    // state is not required: state is atomic.
    void update();

    State state() const { return state_.load(); }
    // Portafilter present and steady long enough to start a grind (or the
    // detector hasn't learned this setup yet, in which case nothing blocks).
    bool ok_to_grind() const;

    static const char* state_name(State state);
    // Short message for the UI when ok_to_grind() is false.
    const char* blocked_reason() const;

private:
    void learn_from_step(float before_g, float after_g);
    void observe_portafilter(float weight_g);
    void classify(float plateau_g, uint32_t now);
    void load();
    void save() const;

    WeightSensor* sensor_ = nullptr;
    uint32_t last_update_ms_ = 0;

    // Current steady plateau (untared grams).
    bool have_plateau_ = false;
    float plateau_g_ = 0.0f;
    uint32_t plateau_since_ms_ = 0;

    // Learned setup.
    bool have_empty_ = false;
    float empty_g_ = 0.0f;
    bool have_band_ = false;
    float band_min_g_ = 0.0f;
    float band_max_g_ = 0.0f;
    float learned_cal_factor_ = 0.0f;

    std::atomic<State> state_{State::UNKNOWN};
    std::atomic<uint32_t> steady_since_ms_{0};
};

extern PortafilterDetector portafilter_detector;
