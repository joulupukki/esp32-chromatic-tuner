/*=============================================================================
    Copyright (c) 2024 Boyd Timothy. All rights reserved.

    Distributed under the MIT License [ https://opensource.org/licenses/MIT ]
=============================================================================*/
#include "tuner_gui_task.h"

#include "defines.h"
#include "globals.h"
#include "tuner_ui_interface.h"
#include "UserSettings.h"

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "esp_adc/adc_continuous.h"
#include "esp_timer.h"

#include <cmath> // for log2()

//
// These are the tuner UIs available.
//
#include "tuner_ui_needle.h"

//
// LVGL Support
//
#include "lvgl.h"
#include "esp_lvgl_port.h"

extern "C" { // because these files are C and not C++
    #include "lcd.h"
    #include "touch.h"
}

static const char *TAG = "GUI";

extern UserSettings *userSettings;
extern "C" const lv_font_t fontawesome_48;

// Local Function Declarations
float midi_note_from_frequency(float freq);
const char * get_pitch_name_and_cents_from_frequency(float freq, float *cents);
void create_main_screen_ui();
void settings_button_cb(lv_event_t *e);
void create_settings_menu_button(lv_obj_t * parent);
static esp_err_t app_lvgl_main();

lv_coord_t screen_width = 0;
lv_coord_t screen_height = 0;

lv_display_t *lvgl_display = NULL;
lv_obj_t *main_screen = NULL;

bool is_gui_loaded = false;

///
/// Add the different GUIs here.
///
TunerGUIInterface needle_gui = {
    .get_id = needle_guit_get_id,
    .get_name = needle_gui_get_name,
    .init = needle_gui_init,
    .display_frequency = needle_gui_display_frequency,
    .cleanup = needle_gui_cleanup
};

TunerGUIInterface available_guis[] = {

    // IMPORTANT: Make sure you update `num_of_available_guis` below so any new
    // Tuner GUI you add here will show up in the user settings as an option.
    
    needle_gui, // ID = 0
};

size_t num_of_available_guis = 1;

TunerGUIInterface *active_gui = NULL;

TunerGUIInterface get_active_gui() {
    uint8_t tuner_gui_index = userSettings->tunerGUIIndex;
    TunerGUIInterface active_gui = available_guis[tuner_gui_index];
    return active_gui;
}

/// @brief The main GUI task.
///
/// This is the main GUI FreeRTOS task and is declared as an extern in main.cpp.
///
/// @param pvParameter User data (unused).
void tuner_gui_task(void *pvParameter) {
    esp_lcd_panel_io_handle_t lcd_io;
    esp_lcd_panel_handle_t lcd_panel;
    esp_lcd_touch_handle_t tp;
    lvgl_port_touch_cfg_t touch_cfg;

    ESP_ERROR_CHECK(lcd_display_brightness_init());

    ESP_ERROR_CHECK(app_lcd_init(&lcd_io, &lcd_panel));
    lvgl_display = app_lvgl_init(lcd_io, lcd_panel);
    if (lvgl_display == NULL)
    {
        ESP_LOGI(TAG, "fatal error in app_lvgl_init");
        esp_restart();
    }

    ESP_ERROR_CHECK(touch_init(&tp));
    touch_cfg.disp = lvgl_display;
    touch_cfg.handle = tp;
    lvgl_port_add_touch(&touch_cfg);

    if (lvgl_port_lock(0)) {
        ESP_ERROR_CHECK(lcd_display_brightness_set(userSettings->displayBrightness * 100));
        ESP_ERROR_CHECK(lcd_display_rotate(lvgl_display, userSettings->getDisplayOrientation()));
        // ESP_ERROR_CHECK(lcd_display_rotate(lvgl_display, LV_DISPLAY_ROTATION_0)); // Upside Down
        lvgl_port_unlock();
    }

    ESP_ERROR_CHECK(app_lvgl_main());

    is_gui_loaded = true;

    float cents;

    while(1) {
        lv_task_handler();

        // Lock the mutex due to the LVGL APIs are not thread-safe
        if (userSettings != NULL && !userSettings->isShowingSettings() && lvgl_port_lock(0)) {
            float frequency = get_current_frequency();
            if (frequency > 0) {
                const char *note_name = get_pitch_name_and_cents_from_frequency(frequency, &cents);
                // ESP_LOGI(TAG, "%s - %d", noteName, cents);
                get_active_gui().display_frequency(frequency, note_name, cents);
            } else {
                get_active_gui().display_frequency(0, NULL, 0);
            }
            // Release the mutex
            lvgl_port_unlock();
        }

        // vTaskDelay(pdMS_TO_TICKS(1)); // ~33 times per second
        vTaskDelay(pdMS_TO_TICKS(125)); // Yields to reset watchdog in milliseconds
        // vTaskDelay(pdMS_TO_TICKS(50)); // Yields to reset watchdog in milliseconds
    }
    vTaskDelay(portMAX_DELAY);
}

void user_settings_updated() {
    if (!is_gui_loaded || !lvgl_port_lock(0)) {
        return;
    }

    screen_width = lv_obj_get_width(main_screen);
    screen_height = lv_obj_get_height(main_screen);

    lvgl_port_unlock();
}

/// This is thread safe (already inside an LVGL lock).
void user_settings_will_exit() {
    // When the user enters user settings, the main tuner UI is destroyed so
    // this builds it back up before they exit.
    create_main_screen_ui();
}

// Function to calculate the MIDI note number from frequency
float midi_note_from_frequency(float freq) {
    return 69 + 12 * log2(freq / A4_FREQ);
}

// Function to get pitch name and cents from MIDI note number
const char * get_pitch_name_and_cents_from_frequency(float freq, float *cents) {
    float midi_note = midi_note_from_frequency(freq);
    int note_index = (int)fmod(midi_note, 12);
    float fractional_note = midi_note - (int)midi_note;


    *cents = (fractional_note * CENTS_PER_SEMITONE);
    if (*cents >= CENTS_PER_SEMITONE / 2) {
        // Adjust the note index so that it still shows the proper note
        note_index++;
        if (note_index >= 12) {
            note_index = 0; // Overflow to the beginning of the note names
        }
        // *cents = *cents - CENTS_PER_SEMITONE;
        *cents -= CENTS_PER_SEMITONE; // Adjust to valid range
    }

    // strncpy(pitch_name, note_names[note_index], strlen(note_names[note_index]));
    if (note_index > 12) { // safeguard to prevent crash of snprintf()
        note_index = 0;
    }
    // pitch_name[strlen(note_names[note_index])] = '\0';
    return note_names[note_index];
}

void create_main_screen_ui() {
    // First build the Tuner UI
    get_active_gui().init(main_screen);

    // Place the settings button on the UI (bottom left)
    create_settings_menu_button(main_screen);
}

void settings_button_cb(lv_event_t *e) {
    ESP_LOGI(TAG, "Settings button clicked");

    // Remove/clean up all existing UI from the main screen because the user may
    // end up choosing a completely different UI. This should also free up
    // memory so the settings menu will have plenty to work with.
    if (!lvgl_port_lock(0)) {
        return;
    }

    get_active_gui().cleanup();
    lv_obj_clean(main_screen);

    lvgl_port_unlock();

    userSettings->showSettings();
}

void create_settings_menu_button(lv_obj_t * parent) {
    lv_obj_t *btn = lv_btn_create(parent);
    lv_obj_set_style_opa(btn, LV_OPA_40, 0);
    lv_obj_remove_style_all(btn);
    lv_obj_set_ext_click_area(btn, 100);
    lv_obj_t *label = lv_label_create(btn);
    lv_label_set_text(label, GEAR_SYMBOL);
    lv_obj_add_event_cb(btn, settings_button_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_align(btn, LV_ALIGN_BOTTOM_LEFT, 20, -20);
}


static esp_err_t app_lvgl_main() {
    // ESP_LOGI("LOCK", "locking in app_lvgl_main");
    lvgl_port_lock(0);
    // ESP_LOGI("LOCK", "locked in app_lvgl_main");

    lv_obj_t *scr = lv_scr_act();
    screen_width = lv_obj_get_width(scr);
    screen_height = lv_obj_get_height(scr);
    lv_obj_set_style_bg_color(scr, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_scrollbar_mode(scr, LV_SCROLLBAR_MODE_OFF);
    main_screen = scr;

    create_main_screen_ui();

    // ESP_LOGI("LOCK", "unlocking in app_lvgl_main");
    lvgl_port_unlock();
    // ESP_LOGI("LOCK", "unlocked in app_lvgl_main");

    userSettings->setDisplayAndScreen(lvgl_display, main_screen);

    return ESP_OK;
}