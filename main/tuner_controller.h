/*=============================================================================
    UserSettings.cpp
    Copyright (c) 2024 Boyd Timothy. All rights reserved.

    Distributed under the MIT License [ https://opensource.org/licenses/MIT ]
=============================================================================*/
#if !defined(TUNER_STATE)
#define TUNER_STATE

#include "freertos/FreeRTOS.h"

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

    TunerController(tuner_state_will_change_cb_t willChange, tuner_state_did_change_cb_t didChange);

    /// @brief Gets the tuner's current state (thread safe).
    /// @return The state.
    TunerState getState();

    /// @brief Sets the tuner state (thread safe).
    /// @param new_state The new state.
    void setState(TunerState new_state);

};

#endif
