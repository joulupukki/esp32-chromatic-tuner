/*=============================================================================
    Copyright (c) 2024 Boyd Timothy. All rights reserved.

    Distributed under the MIT License [ https://opensource.org/licenses/MIT ]
=============================================================================*/
#include "tuner_ui_strobe.h"

#include <stdlib.h>

#include "globals.h"
#include "UserSettings.h"

#include "esp_lvgl_port.h"

extern UserSettings *userSettings;
extern lv_coord_t screen_width;
extern lv_coord_t screen_height;

extern "C" const lv_font_t raleway_128;

//
// Function Definitions
//
void strobe_create_arc(lv_obj_t * parent);
void strobe_create_labels(lv_obj_t * parent);
void strobe_set_note_name_cb(lv_timer_t * timer);
void strobe_update_note_name(const char *new_value);

//
// Local Variables
//
lv_obj_t *strobe_parent_screen = NULL;

// Keep track of the last note that was displayed so telling the UI to update
// can be avoided if it is the same.
const char *strobeLastDisplayedNote = no_freq_name;

lv_timer_t *strobe_note_name_update_timer = NULL;

static lv_obj_t *strobe_note_name_label;
lv_style_t strobe_note_name_label_style;

lv_obj_t *strobe_frequency_label;
lv_style_t strobe_frequency_label_style;
lv_obj_t *strobe_cents_label;
lv_style_t strobe_cents_label_style;

lv_obj_t *strobe_arc;

uint8_t strobe_gui_get_id() {
    return 1;
}

const char * strobe_gui_get_name() {
    return "Strobe";
}

void strobe_gui_init(lv_obj_t *screen) {
    strobe_parent_screen = screen;
    strobe_create_arc(screen);
    strobe_create_labels(screen);

    // Create a new timer that will fire. This will
    // debounce the calls to the UI to update the note name.
    strobe_note_name_update_timer = lv_timer_create(strobe_set_note_name_cb, userSettings->noteDebounceInterval, NULL);
    lv_timer_pause(strobe_note_name_update_timer);
    lv_timer_reset(strobe_note_name_update_timer);
}

void strobe_gui_display_frequency(float frequency, const char *note_name, float cents) {
    if (note_name != NULL) {
        lv_label_set_text_fmt(strobe_frequency_label, "%.2f", frequency);
        lv_obj_clear_flag(strobe_frequency_label, LV_OBJ_FLAG_HIDDEN);

        if (strobeLastDisplayedNote != note_name) {
            strobe_update_note_name(note_name);
            strobeLastDisplayedNote = note_name; // prevent setting this so often to help prevent an LVGL crash
        }

        // Calculate where the indicator bar should be left-to right based on the cents
        lv_coord_t indicator_x_pos = (lv_coord_t)0.0;

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
        // lv_anim_set_values(&pitch_animation, last_pitch_indicator_pos, indicator_x_pos);
        // last_pitch_indicator_pos = indicator_x_pos;

        // Make the two bars show up
        // lv_obj_clear_flag(pitch_indicator_bar, LV_OBJ_FLAG_HIDDEN);

        lv_label_set_text_fmt(strobe_cents_label, "%.1f", cents);
        lv_obj_clear_flag(strobe_cents_label, LV_OBJ_FLAG_HIDDEN);

        // lv_anim_start(&pitch_animation);
    } else {
        // Hide the pitch and indicators since it's not detected
        if (strobeLastDisplayedNote != no_freq_name) {
            strobe_update_note_name(no_freq_name);
            strobeLastDisplayedNote = no_freq_name;
        }

        // Hide the indicator bars and cents label
        // lv_obj_add_flag(pitch_indicator_bar, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(strobe_cents_label, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(strobe_frequency_label, LV_OBJ_FLAG_HIDDEN);
    }
}

void strobe_gui_cleanup() {
    // TODO: Do any cleanup needed here. 
}

void strobe_create_arc(lv_obj_t * parent) {
    // TODO: Create the strobe arc
}

void strobe_create_labels(lv_obj_t * parent) {
    // Note Name Label (the big font at the bottom middle)
    strobe_note_name_label = lv_label_create(parent);
    lv_label_set_long_mode(strobe_note_name_label, LV_LABEL_LONG_CLIP);

    lv_label_set_text_static(strobe_note_name_label, no_freq_name);
    lv_obj_set_width(strobe_note_name_label, screen_width);
    lv_obj_set_style_text_align(strobe_note_name_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(strobe_note_name_label, LV_ALIGN_CENTER, 0, 0);

    lv_style_init(&strobe_note_name_label_style);    
    lv_style_set_text_font(&strobe_note_name_label_style, &raleway_128);
    
    lv_palette_t palette = userSettings->noteNamePalette;
    if (palette == LV_PALETTE_NONE) {
        lv_style_set_text_color(&strobe_note_name_label_style, lv_color_white());
    } else {
        lv_style_set_text_color(&strobe_note_name_label_style, lv_palette_main(palette));
    }

    lv_obj_add_style(strobe_note_name_label, &strobe_note_name_label_style, 0);

    lv_obj_align(strobe_note_name_label, LV_ALIGN_CENTER, 0, 20); // Offset down by 20 pixels

    // Frequency Label (very bottom)
    strobe_frequency_label = lv_label_create(parent);
    lv_label_set_long_mode(strobe_frequency_label, LV_LABEL_LONG_CLIP);

    lv_label_set_text_static(strobe_frequency_label, no_freq_name);
    lv_obj_set_width(strobe_frequency_label, screen_width);
    lv_obj_set_style_text_align(strobe_frequency_label, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_align(strobe_frequency_label, LV_ALIGN_BOTTOM_RIGHT, 0, 0);

    lv_style_init(&strobe_frequency_label_style);
    lv_style_set_text_font(&strobe_frequency_label_style, &lv_font_montserrat_14);
    lv_obj_add_style(strobe_frequency_label, &strobe_frequency_label_style, 0);

    // Cents display
    strobe_cents_label = lv_label_create(parent);
    
    lv_style_init(&strobe_cents_label_style);
    lv_style_set_text_font(&strobe_cents_label_style, &lv_font_montserrat_14);
    lv_obj_add_style(strobe_cents_label, &strobe_cents_label_style, 0);

    lv_obj_set_width(strobe_cents_label, screen_width / 2);
    lv_obj_set_style_text_align(strobe_cents_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(strobe_cents_label, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_add_flag(strobe_cents_label, LV_OBJ_FLAG_HIDDEN);

}

void strobe_set_note_name_cb(lv_timer_t * timer) {
    // The note name is in the timer's user data
    // ESP_LOGI("LOCK", "locking in strobe_set_note_name_cb");
    if (!lvgl_port_lock(0)) {
        return;
    }

    // TODO: Maybe change this to use images instead of const char*
    // because the app is crashing too often with this mechanism.
    const char *note_string = (const char *)lv_timer_get_user_data(timer);
    lv_label_set_text_static(strobe_note_name_label, note_string);

    lv_timer_pause(timer);

    // ESP_LOGI("LOCK", "unlocking in strobe_set_note_name_cb");
    lvgl_port_unlock();
    // ESP_LOGI("LOCK", "unlocked in strobe_set_note_name_cb");
}

void strobe_update_note_name(const char *new_value) {
    // Set the note name with a timer so it doesn't get
    // set too often for LVGL. ADC makes it run SUPER
    // fast and can crash the software.
    lv_timer_set_user_data(strobe_note_name_update_timer, (void *)new_value);
    lv_timer_set_period(strobe_note_name_update_timer, (uint32_t)userSettings->noteDebounceInterval);
    lv_timer_reset(strobe_note_name_update_timer);
    lv_timer_resume(strobe_note_name_update_timer);
}
