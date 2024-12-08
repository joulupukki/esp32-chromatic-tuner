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
#include "tuner_gui_task.h"

extern void tuner_gui_task(void *pvParameter);
extern void pitch_detector_task(void *pvParameter);

UserSettings *userSettings;

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

void user_settings_changed() {
    update_pitch_detector_user_settings();
    update_tuner_user_settings();
}

extern "C" void app_main() {

    // Initialize NVS (Persistent Flash Storage for User Settings)
    userSettings = new UserSettings(user_settings_changed);
    user_settings_changed();

    // Start the Display Task
    xTaskCreatePinnedToCore(
        tuner_gui_task,     // callback function
        "tuner_gui",        // debug name of the task
        32768,              // stack depth (no idea what this should be)
        NULL,               // params to pass to the callback function
        0,                  // ux priority - higher value is higher priority
        NULL,               // handle to the created task - we don't need it
        0                   // Core ID - since we're not using Bluetooth/Wi-Fi, this can be 0 (the protocol CPU)
    );

    // Start the Pitch Reading & Detection Task
    xTaskCreatePinnedToCore(
        pitch_detector_task,    // callback function
        "pitch_detector",       // debug name of the task
        4096,                   // stack depth (no idea what this should be)
        NULL,                   // params to pass to the callback function
        1,                      // This has to be higher than the tuner_gui task or frequency readings aren't as accurate
        NULL,                   // handle to the created task - we don't need it
        1                       // Core ID
    );
}
