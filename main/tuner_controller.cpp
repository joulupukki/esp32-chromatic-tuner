/*=============================================================================
    Copyright (c) 2024 Boyd Timothy. All rights reserved.

    Distributed under the MIT License [ https://opensource.org/licenses/MIT ]
=============================================================================*/
#include "tuner_controller.h"

#include "esp_log.h"

static const char *TAG = "CONTROLLER";

TunerController::TunerController(tuner_state_will_change_cb_t willChange, tuner_state_did_change_cb_t didChange) {
    tunerState = tunerStateBooting;
    stateWillChangeCallback = willChange;
    stateDidChangeCallback = didChange;
}

TunerState TunerController::getState() {
    TunerState state = tunerStateStandby;
    portENTER_CRITICAL(&tuner_state_mutex);
    state = tunerState;
    portEXIT_CRITICAL(&tuner_state_mutex);
    return state;
}

void TunerController::setState(TunerState new_state) {
    TunerState old_state = getState();
    stateWillChangeCallback(old_state, new_state);
    portENTER_CRITICAL(&tuner_state_mutex);
    tunerState = new_state;
    portEXIT_CRITICAL(&tuner_state_mutex);
    stateDidChangeCallback(old_state, new_state);
}