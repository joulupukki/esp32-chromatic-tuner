/*=============================================================================
    Copyright (c) 2024 Boyd Timothy. All rights reserved.

    Distributed under the MIT License [ https://opensource.org/licenses/MIT ]
=============================================================================*/
#include "tuner_ui_strobe.h"

#include <stdlib.h>

#include "globals.h"
#include "UserSettings.h"

#include "esp_lvgl_port.h"

/// The strobe works by changing the rotation of the lv_arc widgets by a certain
/// amount each time it receives a frequency. Changing the rotation too much
/// makes the rotation direction nearly impossible to determine and this keeps
/// that under control.
#define MAX_CENTS_ROTATION 30

extern UserSettings *userSettings;
extern lv_coord_t screen_width;
extern lv_coord_t screen_height;

extern "C" const lv_font_t raleway_128;

//
// Function Definitions
//
void strobe_create_arcs(lv_obj_t * parent);
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

lv_obj_t *strobe_arc_container;
lv_obj_t *strobe_arc1;
lv_obj_t *strobe_arc2;
lv_obj_t *strobe_arc3;

float strobe_rotation_current_pos = 0;

uint8_t strobe_gui_get_id() {
    return 1;
}

const char * strobe_gui_get_name() {
    return "Strobe";
}

void strobe_gui_init(lv_obj_t *screen) {
    strobe_parent_screen = screen;
    strobe_create_arcs(screen);
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

        // Make the strobe arcs show up
        lv_obj_clear_flag(strobe_arc_container, LV_OBJ_FLAG_HIDDEN);

        lv_label_set_text_fmt(strobe_cents_label, "%.1f", cents);
        lv_obj_clear_flag(strobe_cents_label, LV_OBJ_FLAG_HIDDEN);
    } else {
        // Hide the pitch and indicators since it's not detected
        if (strobeLastDisplayedNote != no_freq_name) {
            strobe_update_note_name(no_freq_name);
            strobeLastDisplayedNote = no_freq_name;
        }

        // Hide the strobe arcs, frequency, and cents labels
        lv_obj_add_flag(strobe_arc_container, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(strobe_cents_label, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(strobe_frequency_label, LV_OBJ_FLAG_HIDDEN);
    }

    float amount_to_rotate = cents;
    if (amount_to_rotate > MAX_CENTS_ROTATION) {
        amount_to_rotate = MAX_CENTS_ROTATION;
    } else if (amount_to_rotate < MAX_CENTS_ROTATION) {
        amount_to_rotate = -MAX_CENTS_ROTATION;
    }

    if (amount_to_rotate != 0) {
        strobe_rotation_current_pos += cents; // This will make the strobe rotate left or right depending on how off the tuning is
        lv_arc_set_rotation(strobe_arc1, strobe_rotation_current_pos);
        lv_arc_set_rotation(strobe_arc2, strobe_rotation_current_pos + 120); // 1/3 of a circle ahead
        lv_arc_set_rotation(strobe_arc3, strobe_rotation_current_pos + 240); // 2/3 of a circle ahead
    }
}

void strobe_gui_cleanup() {
    // TODO: Do any cleanup needed here. 
}

void strobe_create_arcs(lv_obj_t * parent) {
    strobe_arc_container = lv_obj_create(parent);
    lv_obj_set_size(strobe_arc_container, lv_pct(100), lv_pct(100));
    lv_obj_set_style_bg_color(strobe_arc_container, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_border_width(strobe_arc_container, 0, 0);
    lv_obj_center(strobe_arc_container);

    strobe_arc1 = lv_arc_create(strobe_arc_container);
    strobe_arc2 = lv_arc_create(strobe_arc_container);
    strobe_arc3 = lv_arc_create(strobe_arc_container);
    // lv_arc_set_rotation(strobe_arc1, 0);
    // lv_arc_set_rotation(strobe_arc2, 120);
    // lv_arc_set_rotation(strobe_arc1, 240);

    lv_obj_remove_style(strobe_arc1, NULL, LV_PART_KNOB); // Don't show a knob
    lv_obj_remove_style(strobe_arc2, NULL, LV_PART_KNOB); // Don't show a knob
    lv_obj_remove_style(strobe_arc3, NULL, LV_PART_KNOB); // Don't show a knob

    lv_obj_remove_flag(strobe_arc1, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_remove_flag(strobe_arc2, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_remove_flag(strobe_arc3, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_set_size(strobe_arc1, 200, 200);
    lv_obj_set_size(strobe_arc2, 200, 200);
    lv_obj_set_size(strobe_arc3, 200, 200);

    lv_obj_set_style_arc_width(strobe_arc1, 20, LV_PART_INDICATOR);
    lv_obj_set_style_arc_width(strobe_arc2, 20, LV_PART_INDICATOR);
    lv_obj_set_style_arc_width(strobe_arc3, 20, LV_PART_INDICATOR);

    lv_obj_center(strobe_arc1);
    lv_obj_center(strobe_arc2);
    lv_obj_center(strobe_arc3);

    lv_arc_set_angles(strobe_arc1, 0, 90);
    lv_arc_set_angles(strobe_arc2, 120, 210); // length of 90 and offset by 1/3 of a circle
    lv_arc_set_angles(strobe_arc3, 240, 330); // length of 90 and offset by 2/3 of a circle

    // Hide the background tracks
    lv_obj_set_style_arc_opa(strobe_arc1, LV_OPA_0, 0);
    lv_obj_set_style_arc_opa(strobe_arc2, LV_OPA_0, 0);
    lv_obj_set_style_arc_opa(strobe_arc3, LV_OPA_0, 0);
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

    lv_obj_align(strobe_note_name_label, LV_ALIGN_CENTER, 0, 0);

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
    if (!lvgl_port_lock(0)) {
        return;
    }

    // TODO: Maybe change this to use images instead of const char*
    // because the app is crashing too often with this mechanism.
    const char *note_string = (const char *)lv_timer_get_user_data(timer);
    lv_label_set_text_static(strobe_note_name_label, note_string);

    lv_timer_pause(timer);

    lvgl_port_unlock();
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