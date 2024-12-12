/*=============================================================================
    Copyright (c) 2024 Boyd Timothy. All rights reserved.

    Distributed under the MIT License [ https://opensource.org/licenses/MIT ]
=============================================================================*/
#include "gpio_task.h"

#include "defines.h"
#include "globals.h"

#include "lvgl.h"
#include "esp_lvgl_port.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "esp_timer.h"

static const char *TAG = "GPIO";

// Keep track of what the relay state is so the app doesn't have to keep making
// calls to set it over and over (which chews up CPU).
uint32_t current_relay_gpio_level = 0; // Off at launch

int footswitch_last_state = 1; // Assume initial state is open (active-high logic and conveniently matches the physical position of the footswitch)
int footswitch_press_count = 0;
int64_t footswitch_start_time = 0;
int64_t footswitch_last_press_time = 0;
bool footswitch_long_press_triggered = false;

//
// Local Function Declarations
//
void configure_gpio_pins();
void handle_gpio_pins();

void gpio_task(void *pvParameter) {
    ESP_LOGI(TAG, "GPIO task started");
    configure_gpio_pins();

    while(1) {
        handle_gpio_pins();

        vTaskDelay(pdMS_TO_TICKS(50)); // Small delay for debouncing
    }
    vTaskDelay(portMAX_DELAY);
}

void configure_gpio_pins() {
    // Configure GPIO 27 as an input pin
    gpio_config_t gpio_27_conf = {
        .pin_bit_mask = (1ULL << FOOT_SWITCH_GPIO),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,   // Enable internal pull-up resistor
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE      // Disable interrupt for this pin
    };

    gpio_config(&gpio_27_conf);

    // Configure GPIO 22 as an output pin
    gpio_config_t gpio_22_conf = {
        .pin_bit_mask = (1ULL << RELAY_GPIO),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };

    gpio_config(&gpio_22_conf);

    // TODO: Read the initial state of the foot switch. If it's a 0, that means
    // the user had it pressed at power up and we may want to do something
    // special.
}

void handle_gpio_pins() {

    // Detect press, double-press, and long press.

    int current_footswitch_state = gpio_get_level(FOOT_SWITCH_GPIO);

    if (current_footswitch_state == 0 && footswitch_last_state == 1) {
        // Button pressed
        footswitch_start_time = esp_timer_get_time() / 1000; // Get time in ms
        int64_t now = footswitch_start_time;

        if ((now - footswitch_last_press_time) <= DOUBLE_CLICK_THRESHOLD) {
            footswitch_press_count++;
        } else {
            footswitch_press_count = 1;
        }

        footswitch_last_press_time = now;
        footswitch_long_press_triggered = false; // Reset long press trigger when pressed
    }

    if (current_footswitch_state == 0) {
        // Button is being held down
        int64_t press_duration = (esp_timer_get_time() / 1000) - footswitch_start_time;

        if (press_duration >= LONG_PRESS_THRESHOLD && !footswitch_long_press_triggered) {
            ESP_LOGI(TAG, "LONG PRESS detected");
            vTaskDelay(pdMS_TO_TICKS(200)); // Small delay for visual feedback
            footswitch_long_press_triggered = true; // Ensure long press is only triggered once
        }
    }

    if (current_footswitch_state == 1 && footswitch_last_state == 0) {
        // Button released
        if (!footswitch_long_press_triggered) {
            int64_t press_duration = (esp_timer_get_time() / 1000) - footswitch_start_time;

            if (footswitch_press_count == 2) {
                ESP_LOGI(TAG, "DOUBLE CLICK detected");
                vTaskDelay(pdMS_TO_TICKS(200)); // Small delay for visual feedback
            } else if (press_duration < LONG_PRESS_THRESHOLD) {
                ESP_LOGI(TAG, "NORMAL PRESS detected");
                vTaskDelay(pdMS_TO_TICKS(200)); // Small delay for visual feedback
            }
        }
    }

    footswitch_last_state = current_footswitch_state;

    // gpio_set_level(RELAY_GPIO, 1); // Turn on GPIO 22
    // current_relay_gpio_level = 1;

    // gpio_set_level(RELAY_GPIO, 0); // Turn off GPIO 22
    // current_relay_gpio_level = 0;
}