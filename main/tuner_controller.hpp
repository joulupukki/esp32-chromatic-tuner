/*=============================================================================
    UserSettings.cpp
    Copyright (c) 2024 Boyd Timothy. All rights reserved.

    Distributed under the MIT License [ https://opensource.org/licenses/MIT ]
=============================================================================*/
#if !defined(TUNER_STATE)
#define TUNER_STATE

#include "esp_log.h"
#include "freertos/FreeRTOS.h"

static const char *CONTROLLER = "CONTROLLER";

enum TunerState: uint8_t {
    tunerStateBooting = 0,
    tunerStateStandby,      // Standby (muted or monitored)
    tunerStateTuning,       // Actively tuning
    tunerStateSettings,     // User settings are showing
};

enum FootswitchPress: uint8_t {
    footswitchNormalPress,
    footswitchDoublePress,
    footswitchLongPress,
};

/// @brief Called before the state of the tuner changes.
/// @param new_state Indicates the state that the tuner is changing to.
typedef void (*tuner_state_will_change_cb_t)(TunerState old_state, TunerState new_state);

/// @brief Called immediately after the state of the tuner changes.
/// @param new_state Indicates the state that the tuner changed to.
typedef void (*tuner_state_did_change_cb_t)(TunerState old_state, TunerState new_state);

class TunerController {

TunerState tunerState;

portMUX_TYPE tuner_state_mutex = portMUX_INITIALIZER_UNLOCKED;

tuner_state_will_change_cb_t    stateWillChangeCallback;
tuner_state_did_change_cb_t     stateDidChangeCallback;

public:

TunerController(tuner_state_will_change_cb_t willChange, tuner_state_did_change_cb_t didChange) {
    tunerState = tunerStateBooting;
    stateWillChangeCallback = willChange;
    stateDidChangeCallback = didChange;
}

/// @brief Gets the tuner's current state (thread safe).
/// @return The state.
TunerState getTunerState() {
    TunerState state = tunerStateStandby;
    portENTER_CRITICAL(&tuner_state_mutex);
    state = tunerState;
    portEXIT_CRITICAL(&tuner_state_mutex);
    return state;
}

/// @brief Sets the tuner state (thread safe).
/// @param new_state The new state.
void setTunerState(TunerState new_state) {
    TunerState old_state = getTunerState();
    stateWillChangeCallback(old_state, new_state);
    portENTER_CRITICAL(&tuner_state_mutex);
    tunerState = new_state;
    portEXIT_CRITICAL(&tuner_state_mutex);
    stateDidChangeCallback(old_state, new_state);
}

void handleFootswitchPress(FootswitchPress press) {
    TunerState state = getTunerState();
    switch (press) {
    case footswitchNormalPress:
        switch (state) {
            case tunerStateSettings:
                // Exit settings and go back to tuning mode

                // Currently this won't be called because watching GPIO while
                // in settings is disabled. If the GPIO task is enabled while
                // settings is showing, NVS calls seem to crash/fail.
                setTunerState(tunerStateTuning);
                break;
            case tunerStateStandby:
                // Go into tuning mode
                setTunerState(tunerStateTuning);
                break;
            case tunerStateTuning:
                setTunerState(tunerStateStandby);
                break;
            default:
                break;
        }
        break;
    case footswitchDoublePress:
        // Do nothing for now
        break;
    case footswitchLongPress:
        // Do nothing for now
        break;
    
    default:
        break;
    }
}

};

#endif
