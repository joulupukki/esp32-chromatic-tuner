/*=============================================================================
    Copyright (c) 2024 Boyd Timothy. All rights reserved.

    Distributed under the MIT License [ https://opensource.org/licenses/MIT ]
=============================================================================*/
#include "tuner_ui_strobe.h"

#include <stdlib.h>

#include "globals.h"
#include "UserSettings.h"

#include "esp_log.h"
#include "esp_lvgl_port.h"

static const char *STROBE = "STROBE";

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
void strobe_create_labels(lv_obj_t * parent);
void strobe_create_arcs(lv_obj_t * parent);
void strobe_update_note_name(TunerNoteName new_value);
void strobe_start_note_fade_animation();
void strobe_stop_note_fade_animation();
void strobe_last_note_anim_cb(lv_obj_t *obj, int32_t value);
void strobe_last_note_anim_completed_cb(lv_anim_t *);

//
// Local Variables
//
lv_obj_t *strobe_parent_screen = NULL;

// Keep track of the last note that was displayed so telling the UI to update
// can be avoided if it is the same.
TunerNoteName strobe_last_displayed_note = NOTE_NONE;

lv_obj_t *strobe_note_img_container;
lv_obj_t *strobe_note_img;
lv_obj_t *strobe_sharp_img;

lv_obj_t *strobe_frequency_label;
lv_style_t strobe_frequency_label_style;
lv_obj_t *strobe_cents_label;
lv_style_t strobe_cents_label_style;

lv_obj_t *strobe_arc_container;
lv_obj_t *strobe_arc1;
lv_obj_t *strobe_arc2;
lv_obj_t *strobe_arc3;

float strobe_rotation_current_pos = 0;

lv_anim_t *strobe_last_note_anim = NULL;

uint8_t strobe_gui_get_id() {
    return 1;
}

const char * strobe_gui_get_name() {
    return "Strobe";
}

void strobe_gui_init(lv_obj_t *screen) {
    strobe_parent_screen = screen;
    strobe_create_labels(screen);
    strobe_create_arcs(screen);
}

void strobe_gui_display_frequency(float frequency, TunerNoteName note_name, float cents) {
    if (note_name < 0) { return; } // Strangely I'm sometimes seeing negative values. No idea how.
    if (note_name != NOTE_NONE) {
        lv_label_set_text_fmt(strobe_frequency_label, "%.2f", frequency);
        lv_obj_clear_flag(strobe_frequency_label, LV_OBJ_FLAG_HIDDEN);

        if (strobe_last_displayed_note != note_name) {
            strobe_update_note_name(note_name);
            strobe_last_displayed_note = note_name; // prevent setting this so often to help prevent an LVGL crash
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
        if (strobe_last_displayed_note != NOTE_NONE) {
            strobe_update_note_name(NOTE_NONE);
            strobe_last_displayed_note = NOTE_NONE;
        }

        // Hide the strobe arcs, frequency, and cents labels
        lv_obj_add_flag(strobe_arc_container, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(strobe_cents_label, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(strobe_frequency_label, LV_OBJ_FLAG_HIDDEN);
    }

    float amount_to_rotate = cents / 2; // Dividing the cents in half for the amount of rotation seems to feel about right
    if (amount_to_rotate != 0) {
        strobe_rotation_current_pos += cents; // This will make the strobe rotate left or right depending on how off the tuning is
        lv_arc_set_rotation(strobe_arc1, strobe_rotation_current_pos);
        lv_arc_set_rotation(strobe_arc2, strobe_rotation_current_pos + 120); // 1/3 of a circle ahead
        lv_arc_set_rotation(strobe_arc3, strobe_rotation_current_pos + 240); // 2/3 of a circle ahead

        // With the UI updating as much as possible, this keeps the tuning
        // stable by yielding for just a teeny amount.
        vTaskDelay(1);
    }
}

void strobe_gui_cleanup() {
    // TODO: Do any cleanup needed here. 
}

void strobe_create_labels(lv_obj_t * parent) {
    // Place the note name and # symbole in the same container so the opacity
    // can be animated.
    strobe_note_img_container = lv_obj_create(parent);
    lv_obj_set_size(strobe_note_img_container, lv_pct(100), lv_pct(100));
    lv_obj_set_style_bg_opa(strobe_note_img_container, LV_OPA_0, 0);
    lv_obj_set_style_border_width(strobe_note_img_container, 0, 0);
    lv_obj_set_scrollbar_mode(strobe_note_img_container, LV_SCROLLBAR_MODE_OFF);
    lv_obj_center(strobe_note_img_container);

    // Note Name Image (the big name in the middle of the screen)
    strobe_note_img = lv_image_create(strobe_note_img_container);
    lv_image_set_src(strobe_note_img, &tuner_font_image_none);
    lv_obj_center(strobe_note_img);

    strobe_sharp_img = lv_image_create(strobe_note_img_container);
    lv_image_set_src(strobe_sharp_img, &tuner_font_image_sharp);
    lv_obj_align_to(strobe_sharp_img, strobe_note_img, LV_ALIGN_TOP_RIGHT, 70, -45);
    lv_obj_add_flag(strobe_sharp_img, LV_OBJ_FLAG_HIDDEN);
    
    // Enable recoloring on the images
    lv_obj_set_style_img_recolor_opa(strobe_note_img, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_img_recolor_opa(strobe_sharp_img, LV_OPA_COVER, LV_PART_MAIN);
    lv_palette_t palette = userSettings->noteNamePalette;
    if (palette == LV_PALETTE_NONE) {
        lv_obj_set_style_img_recolor(strobe_note_img, lv_color_white(), 0);
        lv_obj_set_style_img_recolor(strobe_sharp_img, lv_color_white(), 0);
    } else {
        lv_obj_set_style_img_recolor(strobe_note_img, lv_palette_main(palette), 0);
        lv_obj_set_style_img_recolor(strobe_sharp_img, lv_palette_main(palette), 0);
    }

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
    lv_obj_add_flag(strobe_frequency_label, LV_OBJ_FLAG_HIDDEN);

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

void strobe_create_arcs(lv_obj_t * parent) {
    strobe_arc_container = lv_obj_create(parent);
    lv_obj_set_size(strobe_arc_container, lv_pct(100), lv_pct(100));
    lv_obj_set_style_bg_opa(strobe_arc_container, LV_OPA_0, 0);
    lv_obj_set_style_border_width(strobe_arc_container, 0, 0);
    lv_obj_set_scrollbar_mode(strobe_arc_container, LV_SCROLLBAR_MODE_OFF);
    lv_obj_center(strobe_arc_container);

    strobe_arc1 = lv_arc_create(strobe_arc_container);
    strobe_arc2 = lv_arc_create(strobe_arc_container);
    strobe_arc3 = lv_arc_create(strobe_arc_container);

    lv_obj_remove_style(strobe_arc1, NULL, LV_PART_KNOB); // Don't show a knob
    lv_obj_remove_style(strobe_arc2, NULL, LV_PART_KNOB); // Don't show a knob
    lv_obj_remove_style(strobe_arc3, NULL, LV_PART_KNOB); // Don't show a knob

    lv_obj_remove_flag(strobe_arc1, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_remove_flag(strobe_arc2, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_remove_flag(strobe_arc3, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_set_size(strobe_arc1, 200, 200);
    lv_obj_set_size(strobe_arc2, 200, 200);
    lv_obj_set_size(strobe_arc3, 200, 200);

    lv_obj_set_style_arc_width(strobe_arc1, 14, LV_PART_INDICATOR);
    lv_obj_set_style_arc_width(strobe_arc2, 14, LV_PART_INDICATOR);
    lv_obj_set_style_arc_width(strobe_arc3, 14, LV_PART_INDICATOR);

    lv_obj_set_style_arc_color(strobe_arc1, lv_color_white(), LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(strobe_arc2, lv_color_white(), LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(strobe_arc3, lv_color_white(), LV_PART_INDICATOR);

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

void strobe_update_note_name(TunerNoteName new_value) {
    // Set the note name with a timer so it doesn't get
    // set too often for LVGL. ADC makes it run SUPER
    // fast and can crash the software.
    const lv_image_dsc_t *img_desc;
    bool show_sharp_symbol = false;
    bool show_note_fade_anim = false;
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
    case NOTE_NONE:
        show_note_fade_anim = true;
        break;
    default:
        return;
    }

    if (show_note_fade_anim) {
        strobe_start_note_fade_animation();
    } else {
        strobe_stop_note_fade_animation();

        if (show_sharp_symbol) {
            lv_obj_clear_flag(strobe_sharp_img, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(strobe_sharp_img, LV_OBJ_FLAG_HIDDEN);
        }

        lv_image_set_src(strobe_note_img, img_desc);
    }
}

void strobe_start_note_fade_animation() {
    strobe_last_note_anim = (lv_anim_t *)malloc(sizeof(lv_anim_t));
    lv_anim_init(strobe_last_note_anim);
    lv_anim_set_exec_cb(strobe_last_note_anim, (lv_anim_exec_xcb_t)strobe_last_note_anim_cb);
    lv_anim_set_completed_cb(strobe_last_note_anim, strobe_last_note_anim_completed_cb);
    lv_anim_set_var(strobe_last_note_anim, strobe_note_img_container);
    lv_anim_set_duration(strobe_last_note_anim, LAST_NOTE_FADE_INTERVAL_MS);
    lv_anim_set_values(strobe_last_note_anim, 100, 0); // Fade from 100% to 0% opacity
    lv_anim_start(strobe_last_note_anim);
}

void strobe_stop_note_fade_animation() {
    lv_obj_set_style_opa(strobe_note_img_container, LV_OPA_100, 0);
    if (strobe_last_note_anim == NULL) {
        return;
    }
    lv_anim_del_all();
    delete(strobe_last_note_anim);
    strobe_last_note_anim = NULL;
}

void strobe_last_note_anim_cb(lv_obj_t *obj, int32_t value) {
    if (!lvgl_port_lock(0)) {
        return;
    }

    lv_obj_set_style_opa(obj, value, LV_PART_MAIN);

    lvgl_port_unlock();
}

void strobe_last_note_anim_completed_cb(lv_anim_t *) {
    if (!lvgl_port_lock(0)) {
        return;
    }
    // The animation has completed so hide the note name and set
    // the opacity back to 100%.
    lv_obj_add_flag(strobe_sharp_img, LV_OBJ_FLAG_HIDDEN);
    lv_image_set_src(strobe_note_img, &tuner_font_image_none);
    strobe_last_displayed_note = NOTE_NONE;

    strobe_stop_note_fade_animation();

    lvgl_port_unlock();
}