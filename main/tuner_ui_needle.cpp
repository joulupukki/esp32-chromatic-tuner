/*=============================================================================
    Copyright (c) 2024 Boyd Timothy. All rights reserved.

    Distributed under the MIT License [ https://opensource.org/licenses/MIT ]
=============================================================================*/
#include "tuner_ui_needle.h"

#include <stdlib.h>

#include "globals.h"
#include "UserSettings.h"

#include "esp_lvgl_port.h"

#define PITCH_INDICATOR_BAR_WIDTH       8

extern UserSettings *userSettings;
extern lv_coord_t screen_width;
extern lv_coord_t screen_height;

LV_IMG_DECLARE(tuner_font_image_a)
LV_IMG_DECLARE(tuner_font_image_b)
LV_IMG_DECLARE(tuner_font_image_c)
LV_IMG_DECLARE(tuner_font_image_d)
LV_IMG_DECLARE(tuner_font_image_e)
LV_IMG_DECLARE(tuner_font_image_f)
LV_IMG_DECLARE(tuner_font_image_g)
LV_IMG_DECLARE(tuner_font_image_none)
LV_IMG_DECLARE(tuner_font_image_sharp)

//
// Function Definitions
//
void needle_create_ruler(lv_obj_t * parent);
void needle_create_labels(lv_obj_t * parent);
void needle_update_note_name(TunerNoteName new_value);

//
// Local Variables
//
lv_obj_t *needle_parent_screen = NULL;

// Keep track of the last note that was displayed so telling the UI to update
// can be avoided if it is the same.
TunerNoteName needle_last_displayed_note = NOTE_NONE;

lv_obj_t *needle_note_name_img;
lv_obj_t *needle_sharp_img;

lv_anim_t needle_pitch_animation;
lv_coord_t needle_last_pitch_indicator_pos = (lv_coord_t)0.0;
lv_timer_t *needle_note_name_update_timer = NULL;

lv_obj_t *needle_frequency_label;
lv_style_t needle_frequency_label_style;
lv_obj_t *needle_cents_label;
lv_style_t needle_cents_label_style;

lv_obj_t *needle_pitch_indicator_bar;

uint8_t needle_gui_get_id() {
    return 0;
}

const char * needle_gui_get_name() {
    return "Needle";
}

void needle_gui_init(lv_obj_t *screen) {
    needle_parent_screen = screen;
    needle_create_ruler(screen);
    needle_create_labels(screen);
}

void needle_gui_display_frequency(float frequency, TunerNoteName note_name, float cents) {
    if (note_name != NOTE_NONE) {
        lv_label_set_text_fmt(needle_frequency_label, "%.2f", frequency);
        lv_obj_clear_flag(needle_frequency_label, LV_OBJ_FLAG_HIDDEN);

        if (needle_last_displayed_note != note_name) {
            needle_update_note_name(note_name);
            needle_last_displayed_note = note_name; // prevent setting this so often to help prevent an LVGL crash
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

        lv_anim_set_values(&needle_pitch_animation, needle_last_pitch_indicator_pos, indicator_x_pos);
        needle_last_pitch_indicator_pos = indicator_x_pos;

        // Make the two bars show up
        lv_obj_clear_flag(needle_pitch_indicator_bar, LV_OBJ_FLAG_HIDDEN);

        lv_label_set_text_fmt(needle_cents_label, "%.1f", cents);
        lv_obj_clear_flag(needle_cents_label, LV_OBJ_FLAG_HIDDEN);

        lv_anim_start(&needle_pitch_animation);
    } else {
        // Hide the pitch and indicators since it's not detected
        if (needle_last_displayed_note != NOTE_NONE) {
            needle_update_note_name(NOTE_NONE);
            needle_last_displayed_note = NOTE_NONE;
        }

        // Hide the indicator bar, frequency, and cents labels
        lv_obj_add_flag(needle_pitch_indicator_bar, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(needle_cents_label, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(needle_frequency_label, LV_OBJ_FLAG_HIDDEN);
    }
}

void needle_gui_cleanup() {
    // TODO: Do any cleanup needed here. 
}

void needle_create_ruler(lv_obj_t * parent) {
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

    needle_cents_label = lv_label_create(cents_container);
    
    lv_style_init(&needle_cents_label_style);
    lv_style_set_text_font(&needle_cents_label_style, &lv_font_montserrat_14);
    lv_obj_add_style(needle_cents_label, &needle_cents_label_style, 0);

    lv_obj_set_width(needle_cents_label, screen_width / 2);
    lv_obj_set_style_text_align(needle_cents_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(needle_cents_label, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_add_flag(needle_cents_label, LV_OBJ_FLAG_HIDDEN);

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

    needle_pitch_indicator_bar = rect;

    // Hide these bars initially
    lv_obj_add_flag(needle_pitch_indicator_bar, LV_OBJ_FLAG_HIDDEN);

    // Initialize the pitch animation
    lv_anim_init(&needle_pitch_animation);
    lv_anim_set_exec_cb(&needle_pitch_animation, (lv_anim_exec_xcb_t) lv_obj_set_x);
    lv_anim_set_var(&needle_pitch_animation, needle_pitch_indicator_bar);
    lv_anim_set_duration(&needle_pitch_animation, 150);
}

void needle_create_labels(lv_obj_t * parent) {
    needle_note_name_img = lv_image_create(parent);
    lv_image_set_src(needle_note_name_img, &tuner_font_image_none);
    lv_obj_align(needle_note_name_img, LV_ALIGN_CENTER, 0, 20); // Offset down by 20 pixels

    needle_sharp_img = lv_image_create(parent);
    lv_image_set_src(needle_sharp_img, &tuner_font_image_sharp);

    lv_obj_align_to(needle_sharp_img, needle_note_name_img, LV_ALIGN_TOP_RIGHT, 20, -15);
    lv_obj_add_flag(needle_sharp_img, LV_OBJ_FLAG_HIDDEN);
    
    // Enable recoloring on the images
    lv_obj_set_style_img_recolor_opa(needle_note_name_img, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_img_recolor_opa(needle_sharp_img, LV_OPA_COVER, LV_PART_MAIN);
    lv_palette_t palette = userSettings->noteNamePalette;
    if (palette == LV_PALETTE_NONE) {
        lv_obj_set_style_img_recolor(needle_note_name_img, lv_color_white(), 0);
        lv_obj_set_style_img_recolor(needle_sharp_img, lv_color_white(), 0);
    } else {
        lv_obj_set_style_img_recolor(needle_note_name_img, lv_palette_main(palette), 0);
        lv_obj_set_style_img_recolor(needle_sharp_img, lv_palette_main(palette), 0);
    }

    // Frequency Label (very bottom)
    needle_frequency_label = lv_label_create(parent);
    lv_label_set_long_mode(needle_frequency_label, LV_LABEL_LONG_CLIP);

    lv_label_set_text_static(needle_frequency_label, no_freq_name);
    lv_obj_set_width(needle_frequency_label, screen_width);
    lv_obj_set_style_text_align(needle_frequency_label, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_align(needle_frequency_label, LV_ALIGN_BOTTOM_RIGHT, 0, 0);

    lv_style_init(&needle_frequency_label_style);
    lv_style_set_text_font(&needle_frequency_label_style, &lv_font_montserrat_14);
    lv_obj_add_style(needle_frequency_label, &needle_frequency_label_style, 0);
}

void needle_update_note_name(TunerNoteName new_value) {
    // Set the note name with a timer so it doesn't get
    // set too often for LVGL. ADC makes it run SUPER
    // fast and can crash the software.
    const lv_image_dsc_t *img_desc;
    bool show_sharp_symbol = false;
    switch (new_value) {
    case NOTE_A_SHARP:
        show_sharp_symbol = true;
    case NOTE_A:
        img_desc = &tuner_font_image_a;
        break;
    case NOTE_B:
        img_desc = &tuner_font_image_b;
        break;
    case NOTE_C_SHARP:
        show_sharp_symbol = true;
    case NOTE_C:
        img_desc = &tuner_font_image_c;
        break;
    case NOTE_D_SHARP:
        show_sharp_symbol = true;
    case NOTE_D:
        img_desc = &tuner_font_image_d;
        break;
    case NOTE_E:
        img_desc = &tuner_font_image_e;
        break;
    case NOTE_F_SHARP:
        show_sharp_symbol = true;
    case NOTE_F:
        img_desc = &tuner_font_image_f;
        break;
    case NOTE_G_SHARP:
        show_sharp_symbol = true;
    case NOTE_G:
        img_desc = &tuner_font_image_g;
        break;
    default:
        img_desc = &tuner_font_image_none;
        break;
    }

    if (show_sharp_symbol) {
        lv_obj_remove_flag(needle_sharp_img, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(needle_sharp_img, LV_OBJ_FLAG_HIDDEN);
    }

    lv_image_set_src(needle_note_name_img, img_desc);
}
