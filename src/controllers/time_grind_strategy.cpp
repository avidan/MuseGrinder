#include "time_grind_strategy.h"
#include "../config/constants.h"
#include "grind_controller.h"
#include "../logging/grind_logging.h"
#include <Arduino.h>

void TimeGrindStrategy::on_enter(const GrindSessionDescriptor&,
                                 GrindStrategyContext& context,
                                 const GrindLoopData&) {
    if (!context.controller) {
        return;
    }
    context.controller->time_grind_start_ms = 0;
}

bool TimeGrindStrategy::update(const GrindSessionDescriptor& session,
                               GrindStrategyContext& context,
                               const GrindLoopData& loop_data) {
    auto* controller = context.controller;
    if (!controller || session.mode != GrindMode::TIME) {
        return false;
    }

    switch (controller->phase) {
        case GrindPhase::TIME_GRINDING: {
            if (controller->time_grind_start_ms == 0) {
                controller->time_grind_start_ms = loop_data.now;
            }

            if (controller->grind_paused_) {
                return true;
            }

            if (controller->target_time_ms == 0) {
                controller->grinder->stop();
                controller->switch_phase(GrindPhase::FINAL_SETTLING, loop_data);
                return true;
            }

            long elapsed = static_cast<long>(loop_data.now - controller->time_grind_start_ms)
                         - static_cast<long>(controller->total_pause_ms_);
            if (elapsed >= static_cast<long>(controller->target_time_ms)) {
                controller->grinder->stop();
                controller->switch_phase(GrindPhase::FINAL_SETTLING, loop_data);
            }
            return true;
        }
        default:
            return false;
    }
}

void TimeGrindStrategy::on_exit(const GrindSessionDescriptor&, GrindStrategyContext& context) {
    if (context.controller) {
        context.controller->time_grind_start_ms = 0;
    }
}

int TimeGrindStrategy::progress_percent(const GrindSessionDescriptor& session,
                                        const GrindController& controller) const {
    if (session.mode != GrindMode::TIME || session.target_time_ms == 0) {
        return 0;
    }

    if (controller.time_grind_start_ms == 0) {
        return 0;
    }

    unsigned long current_pause_ms = (controller.grind_paused_ && controller.pause_start_ms_ > 0)
                                   ? (millis() - controller.pause_start_ms_) : 0;
    long elapsed = static_cast<long>(millis() - controller.time_grind_start_ms)
                 - static_cast<long>(controller.total_pause_ms_) - static_cast<long>(current_pause_ms);
    if (elapsed <= 0) {
        return 0;
    }
    if (elapsed >= static_cast<long>(session.target_time_ms)) {
        return 100;
    }

    int progress = static_cast<int>((static_cast<float>(elapsed) / static_cast<float>(session.target_time_ms)) * 100.0f);
    if (progress < 0) progress = 0;
    if (progress > 100) progress = 100;
    return progress;
}
