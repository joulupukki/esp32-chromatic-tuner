/*=============================================================================
    Copyright (c) 2024 Boyd Timothy. All rights reserved.

    Distributed under the MIT License [ https://opensource.org/licenses/MIT ]
=============================================================================*/
#include "tuner_gui_task.h"

#include "defines.h"
#include "globals.h"
#include "UserSettings.h"

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "esp_adc/adc_continuous.h"
#include "esp_timer.h"

#include <cmath> // for log2()

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
extern "C" const lv_font_t raleway_128;
extern "C" const lv_font_t fontawesome_48;

bool is_ui_loaded = false;

static const char *note_names[] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
static const char *no_freq_name = "-";
lv_anim_t pitch_animation;
lv_coord_t last_pitch_indicator_pos = (lv_coord_t)0.0;
lv_timer_t *note_name_update_timer = NULL;

static lv_obj_t *note_name_label;
lv_style_t note_name_label_style;

lv_obj_t *frequency_label;
lv_style_t frequency_label_style;
lv_obj_t *cents_label;
lv_style_t cents_label_style;

lv_obj_t *pitch_indicator_bar;
lv_coord_t screen_width = 0;
lv_coord_t screen_height = 0;
lv_display_t *lvgl_display = NULL;

lv_obj_t *main_screen = NULL;

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

void create_labels(lv_obj_t * parent) {
    // Note Name Label (the big font at the bottom middle)
    note_name_label = lv_label_create(parent);
    lv_label_set_long_mode(note_name_label, LV_LABEL_LONG_CLIP);

    lv_label_set_text_static(note_name_label, no_freq_name);
    lv_obj_set_width(note_name_label, screen_width);
    lv_obj_set_style_text_align(note_name_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(note_name_label, LV_ALIGN_CENTER, 0, 0);

    lv_style_init(&note_name_label_style);    
    lv_style_set_text_font(&note_name_label_style, &raleway_128);
    
    lv_palette_t palette = userSettings->noteNamePalette;
    if (palette == LV_PALETTE_NONE) {
        lv_style_set_text_color(&note_name_label_style, lv_color_white());
    } else {
        lv_style_set_text_color(&note_name_label_style, lv_palette_main(palette));
    }

    lv_obj_add_style(note_name_label, &note_name_label_style, 0);

    lv_obj_align(note_name_label, LV_ALIGN_CENTER, 0, 20); // Offset down by 20 pixels

    // Frequency Label (very bottom)
    frequency_label = lv_label_create(parent);
    lv_label_set_long_mode(frequency_label, LV_LABEL_LONG_CLIP);

    lv_label_set_text_static(frequency_label, no_freq_name);
    lv_obj_set_width(frequency_label, screen_width);
    lv_obj_set_style_text_align(frequency_label, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_align(frequency_label, LV_ALIGN_BOTTOM_RIGHT, 0, 0);

    lv_style_init(&frequency_label_style);
    lv_style_set_text_font(&frequency_label_style, &lv_font_montserrat_14);
    lv_obj_add_style(frequency_label, &frequency_label_style, 0);
}

void settings_button_cb(lv_event_t *e) {
    ESP_LOGI(TAG, "Settings button clicked");
    userSettings->showSettings();
}

void create_settings_menu_button(lv_obj_t * parent) {
    lv_obj_t *scr = lv_scr_act();
    lv_obj_t *btn = lv_btn_create(scr);
    lv_obj_set_style_opa(btn, LV_OPA_40, 0);
    lv_obj_remove_style_all(btn);
    lv_obj_set_ext_click_area(btn, 100);
    lv_obj_t *label = lv_label_create(btn);
    lv_label_set_text(label, GEAR_SYMBOL);
    lv_obj_add_event_cb(btn, settings_button_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_align(btn, LV_ALIGN_BOTTOM_LEFT, 20, -20);
}

void create_ruler(lv_obj_t * parent) {
    const int ruler_height = 50;     // Total height of the ruler
    const int ruler_line_width = 2;  // Width of each ruler line
    const int spacer_width = (screen_width - (29 * ruler_line_width)) / 30;      // Width between items
    const int center_height = 40;    // Height of the center line
    const int tall_height = 30;      // Height of taller side lines
    const int short_height = 20;     // Height of shorter side lines
    const int num_lines_side = 14;   // Number of lines on each side of the center

    const int cents_container_height = ruler_height + 28;

    // Cents Label (shown right under the pitch indicator bar)
    lv_obj_t * cents_container = lv_obj_create(parent);
    lv_obj_set_scrollbar_mode(cents_container, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_style_border_width(cents_container, 0, LV_PART_MAIN);
    lv_obj_set_size(cents_container, screen_width, cents_container_height);
    lv_obj_set_style_bg_color(cents_container, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(cents_container, LV_OPA_COVER, 0);
    lv_obj_align(cents_container, LV_ALIGN_TOP_MID, 0, 0);

    cents_label = lv_label_create(cents_container);
    
    lv_style_init(&cents_label_style);
    lv_style_set_text_font(&cents_label_style, &lv_font_montserrat_14);
    lv_obj_add_style(cents_label, &cents_label_style, 0);

    lv_obj_set_width(cents_label, screen_width / 2);
    lv_obj_set_style_text_align(cents_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(cents_label, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_add_flag(cents_label, LV_OBJ_FLAG_HIDDEN);


    // Create a container for the ruler
    lv_obj_t * ruler_container = lv_obj_create(parent);
    lv_obj_set_scrollbar_mode(ruler_container, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_style_border_width(ruler_container, 0, LV_PART_MAIN);
    lv_obj_set_size(ruler_container, screen_width, ruler_height);
    lv_obj_set_style_bg_color(ruler_container, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(ruler_container, LV_OPA_COVER, 0);
    lv_obj_align(ruler_container, LV_ALIGN_TOP_MID, 0, 0);

    // Center line
    lv_obj_t * center_line = lv_obj_create(ruler_container);
    lv_obj_set_size(center_line, ruler_line_width, center_height);
    lv_obj_set_style_bg_color(center_line, lv_color_hex(0x777777), 0);
    lv_obj_set_style_bg_opa(center_line, LV_OPA_COVER, 0);
    lv_obj_align(center_line, LV_ALIGN_CENTER, 0, 0);

    // Lines on the left and right sides
    for (int i = 1; i <= num_lines_side; i++) {
        // Calculate heights for alternating lines
        int line_height = ((i % 2 == 0) ? tall_height : short_height) - i;
        bool is_tall_height = i % 2;

        // Left side line
        lv_obj_t * left_line = lv_obj_create(ruler_container);
        lv_obj_set_size(left_line, ruler_line_width, line_height);
        lv_obj_set_style_bg_color(left_line, lv_color_hex(is_tall_height ? 0x777777 : 0x333333), 0);
        lv_obj_set_style_bg_opa(left_line, LV_OPA_COVER, 0);
        lv_obj_align(left_line, LV_ALIGN_CENTER, -(spacer_width + 2) * i, 0);

        // Right side line
        lv_obj_t * right_line = lv_obj_create(ruler_container);
        lv_obj_set_size(right_line, ruler_line_width, line_height);
        lv_obj_set_style_bg_color(right_line, lv_color_hex(is_tall_height ? 0x777777 : 0x333333), 0);
        lv_obj_set_style_bg_opa(right_line, LV_OPA_COVER, 0);
        lv_obj_align(right_line, LV_ALIGN_CENTER, (spacer_width + 2) * i, 0);
    }

    //
    // Create the indicator bar. This is the bar
    // that moves around during tuning.
    lv_obj_t * rect = lv_obj_create(ruler_container);

    // Set the rectangle's size and position
    lv_obj_set_size(rect, PITCH_INDICATOR_BAR_WIDTH, center_height);
    lv_obj_set_style_border_width(rect, 0, LV_PART_MAIN);
    lv_obj_align(rect, LV_ALIGN_CENTER, 0, 0);

    // Set the rectangle's style (optional)
    lv_obj_set_style_bg_color(rect, lv_color_hex(0xFF0000), LV_PART_MAIN);

    pitch_indicator_bar = rect;

    // Hide these bars initially
    lv_obj_add_flag(pitch_indicator_bar, LV_OBJ_FLAG_HIDDEN);

    // Initialize the pitch animation
    lv_anim_init(&pitch_animation);
    lv_anim_set_exec_cb(&pitch_animation, (lv_anim_exec_xcb_t) lv_obj_set_x);
    lv_anim_set_var(&pitch_animation, pitch_indicator_bar);
    lv_anim_set_duration(&pitch_animation, 150);
}

// This function is for debug and will just show the frequency and nothing else
const char *lastDisplayedNote = no_freq_name;

void set_note_name_cb(lv_timer_t * timer) {
    // The note name is in the timer's user data
    // ESP_LOGI("LOCK", "locking in set_note_name_cb");
    if (!lvgl_port_lock(0)) {
        return;
    }
    const char *note_string = (const char *)lv_timer_get_user_data(timer);
    lv_label_set_text_static(note_name_label, note_string);

    lv_timer_pause(timer);

    // ESP_LOGI("LOCK", "unlocking in set_note_name_cb");
    lvgl_port_unlock();
    // ESP_LOGI("LOCK", "unlocked in set_note_name_cb");
}

void update_note_name(const char *new_value) {
    // Set the note name with a timer so it doesn't get
    // set too often for LVGL. ADC makes it run SUPER
    // fast and can crash the software.
    lv_timer_set_user_data(note_name_update_timer, (void *)new_value);
    lv_timer_set_period(note_name_update_timer, (uint32_t)userSettings->noteDebounceInterval);
    lv_timer_reset(note_name_update_timer);
    lv_timer_resume(note_name_update_timer);
}

void display_pitch(float frequency, const char *noteName, float cents) {
    if (noteName != NULL) {
        lv_label_set_text_fmt(frequency_label, "%.2f", frequency);
        lv_obj_clear_flag(frequency_label, LV_OBJ_FLAG_HIDDEN);

        // Show a noteName with indicators
        if (lastDisplayedNote != noteName) {
            update_note_name(noteName);
            lastDisplayedNote = noteName; // prevent setting this so often to help prevent an LVGL crash
        }

        // Calculate where the indicator bar should be left-to right based on the cents
        lv_coord_t indicator_x_pos = (lv_coord_t)0.0;
        // auto cents_per_side = CENTS_PER_SEMITONE / 2;
        // lv_coord_t half_width = screen_width / 2;

        if (abs(cents) <= userSettings->inTuneCentsWidth / 2) {
            // Show this as perfectly in tune
            indicator_x_pos = 0;
        } else {
            float segment_width_cents = CENTS_PER_SEMITONE / INDICATOR_SEGMENTS; // TODO: Make this a static var
            int segment_index = cents / segment_width_cents;

            float segment_width_pixels = screen_width / INDICATOR_SEGMENTS;
            indicator_x_pos = segment_index * segment_width_pixels; 
        }

        // lv_obj_align(pitch_indicator_bar, LV_ALIGN_TOP_MID, indicator_x_pos, 18);
        lv_anim_set_values(&pitch_animation, last_pitch_indicator_pos, indicator_x_pos);
        last_pitch_indicator_pos = indicator_x_pos;

        // Make the two bars show up
        lv_obj_clear_flag(pitch_indicator_bar, LV_OBJ_FLAG_HIDDEN);

        lv_label_set_text_fmt(cents_label, "%.1f", cents);
        // lv_obj_align(cents_label, LV_ALIGN_CENTER, indicator_x_pos, 4);
        lv_obj_clear_flag(cents_label, LV_OBJ_FLAG_HIDDEN);

        lv_anim_start(&pitch_animation);
    } else {
        // Hide the pitch and indicators since it's not detected
        if (lastDisplayedNote != no_freq_name) {
            update_note_name(no_freq_name);
            lastDisplayedNote = no_freq_name;
        }

        // Hide the indicator bars and cents label
        lv_obj_add_flag(pitch_indicator_bar, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(cents_label, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(frequency_label, LV_OBJ_FLAG_HIDDEN);
    }
}

static esp_err_t app_lvgl_main() {
    // ESP_LOGI("LOCK", "locking in app_lvgl_main");
    lvgl_port_lock(0);
    // ESP_LOGI("LOCK", "locked in app_lvgl_main");

    lv_obj_t *scr = lv_scr_act();
    screen_width = lv_obj_get_width(scr);
    screen_height = lv_obj_get_height(scr);
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x000000), LV_PART_MAIN);
    lv_obj_set_scrollbar_mode(scr, LV_SCROLLBAR_MODE_OFF);
    create_ruler(scr);
    create_labels(scr);
    create_settings_menu_button(scr);
    main_screen = scr;

    // Create a new timer that will fire. This will
    // debounce the calls to the UI to update the note name.
    note_name_update_timer = lv_timer_create(set_note_name_cb, userSettings->noteDebounceInterval, NULL);
    lv_timer_pause(note_name_update_timer);
    lv_timer_reset(note_name_update_timer);

    // ESP_LOGI("LOCK", "unlocking in app_lvgl_main");
    lvgl_port_unlock();
    // ESP_LOGI("LOCK", "unlocked in app_lvgl_main");

    userSettings->setDisplayAndScreen(lvgl_display, main_screen);

    return ESP_OK;
}

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

    is_ui_loaded = true;

    float cents;

    while(1) {
        lv_task_handler();

        // Lock the mutex due to the LVGL APIs are not thread-safe
        if (userSettings != NULL && !userSettings->isShowingSettings() && lvgl_port_lock(0)) {
            float frequency = get_current_frequency();
            if (frequency > 0) {
                const char *noteName = get_pitch_name_and_cents_from_frequency(frequency, &cents);
                // ESP_LOGI(TAG, "%s - %d", noteName, cents);
                display_pitch(frequency, noteName, cents);
            } else {
                display_pitch(0, NULL, 0);
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

void update_tuner_user_settings() {
    if (!is_ui_loaded) {
        return;
    }

    if (!lvgl_port_lock(0)) {
        return;
    }

    lv_palette_t palette = userSettings->noteNamePalette;
    if (palette == LV_PALETTE_NONE) {
        lv_obj_set_style_text_color(note_name_label, lv_color_white(), 0);
    } else {
        lv_obj_set_style_text_color(note_name_label, lv_palette_main(palette), 0);
    }

    lvgl_port_unlock();
}
