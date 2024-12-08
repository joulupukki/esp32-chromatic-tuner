/*=============================================================================
    Copyright (c) 2024 Boyd Timothy. All rights reserved.

    Distributed under the MIT License [ https://opensource.org/licenses/MIT ]
=============================================================================*/
#include "globals.h"

#include "freertos/FreeRTOS.h"

float current_frequency = -1.0f;
portMUX_TYPE current_frequency_mutex = portMUX_INITIALIZER_UNLOCKED;

float get_current_frequency() {
    float frequency = -1.0f;
    portENTER_CRITICAL(&current_frequency_mutex);
    frequency = current_frequency;
    portEXIT_CRITICAL(&current_frequency_mutex);
    return frequency;
}

void set_current_frequency(float new_frequency) {
    portENTER_CRITICAL(&current_frequency_mutex);
    current_frequency = new_frequency;
    portEXIT_CRITICAL(&current_frequency_mutex);
}