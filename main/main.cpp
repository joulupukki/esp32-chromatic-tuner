/*=============================================================================
    Copyright (c) 2024 Boyd Timothy. All rights reserved.

    Distributed under the MIT License [ https://opensource.org/licenses/MIT ]
=============================================================================*/
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "esp_adc/adc_continuous.h"
#include "esp_timer.h"

#include "defines.h"
#include "globals.h"
#include "UserSettings.h"
#include "pitch_detector_task.h"
#include "tuner_controller.h"
#include "tuner_gui_task.h"

extern void gpio_task(void *pvParameter);
extern void tuner_gui_task(void *pvParameter);
extern void pitch_detector_task(void *pvParameter);

TunerController *tunerController;
UserSettings *userSettings;

TaskHandle_t gpioTaskHandle;
TaskHandle_t detectorTaskHandle;

/* GPIO PINS

P3:
GND - Not used
GPIO 35 (ADC1_CH7) - Input of the amplified guitar signal here
GPIO 22 - Control signal to the non-latching relay
GPIO 21 - This is not available since it's always ON when the LCD backlight is on

CN1:
GND - Not used
GPIO 22 - Same as P3
GPIO 27 - Momentary foot switch input
3V3 - Not used

*/

static const char *TAG = "TUNER";

void tuner_state_will_change_cb(TunerState old_state, TunerState new_state) {
    ESP_LOGI(TAG, "tuner_state_will_change_cb: %d > %d", old_state, new_state);
}

void tuner_state_did_change_cb(TunerState old_state, TunerState new_state) {
    // Suspend and resume tasks as needed.
    switch (new_state) {
    case tunerStateSettings:
        vTaskSuspend(gpioTaskHandle); // Without pausing this NVS failed to work (crashes the app)
        vTaskSuspend(detectorTaskHandle);
        break;
    case tunerStateStandby:
        vTaskSuspend(detectorTaskHandle);
        break;
    case tunerStateTuning:
        // eTaskState gpioTaskState = eTaskGetState(gpioTaskHandle);
        // if (gpioTaskState == eSuspended) {
        // }
        vTaskResume(gpioTaskHandle);
        vTaskResume(detectorTaskHandle);
        break;
    case tunerStateBooting:
        break;
    }

    // Tell the UI about the update so it can update.
    tuner_gui_task_tuner_state_changed(old_state, new_state);
}

void user_settings_will_show_cb() {
}

void user_settings_changed_cb() {
    update_pitch_detector_user_settings();
    user_settings_updated();
}

/// @brief Called right before user settings exits back to the main tuner UI.
void user_settings_will_exit_cb() {
}

extern "C" void app_main() {

    // Initialize NVS (Persistent Flash Storage for User Settings)
    userSettings = new UserSettings(user_settings_will_show_cb, user_settings_changed_cb, user_settings_will_exit_cb);
    user_settings_changed_cb(); // Calling this allows the pitch detector and tuner UI to initialize properly with current user

    tunerController = new TunerController(tuner_state_will_change_cb, tuner_state_did_change_cb);

    // Start the GPIO Task
    xTaskCreatePinnedToCore(
        gpio_task,          // callback function
        "gpio",             // debug name of the task
        2048,               // stack depth (no idea what this should be)
        NULL,               // params to pass to the callback function
        0,                  // ux priority - higher value is higher priority
        &gpioTaskHandle,    // handle to the created task - we don't need it
        0                   // Core ID - since we're not using Bluetooth/Wi-Fi, this can be 0 (the protocol CPU)
    );

    // Start the Display Task
    xTaskCreatePinnedToCore(
        tuner_gui_task,     // callback function
        "tuner_gui",        // debug name of the task
        16384,              // stack depth (no idea what this should be)
        NULL,               // params to pass to the callback function
        1,                  // ux priority - higher value is higher priority
        NULL,               // handle to the created task - we don't need it
        0                   // Core ID - since we're not using Bluetooth/Wi-Fi, this can be 0 (the protocol CPU)
    );

    // Start the Pitch Reading & Detection Task
    xTaskCreatePinnedToCore(
        pitch_detector_task,    // callback function
        "pitch_detector",       // debug name of the task
        4096,                   // stack depth (no idea what this should be)
        NULL,                   // params to pass to the callback function
        10,                     // This has to be higher than the tuner_gui task or frequency readings aren't as accurate
        &detectorTaskHandle,    // handle to the created task - we don't need it
        1                       // Core ID
    );
}
