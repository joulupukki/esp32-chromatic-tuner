/*=============================================================================
    Copyright (c) 2024 Boyd Timothy. All rights reserved.

    Distributed under the MIT License [ https://opensource.org/licenses/MIT ]
=============================================================================*/
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "esp_adc/adc_continuous.h"
// #include "hal/adc_ll.h"
#include "esp_timer.h"

#include "exponential_smoother.hpp"
#include "OneEuroFilter.h"

#include "UserSettings.h"

//
// Q DSP Library for Pitch Detection
//
#include <q/pitch/pitch_detector.hpp>
#include <q/fx/dynamic.hpp>
#include <q/fx/clip.hpp>
#include <q/fx/signal_conditioner.hpp>
#include <q/support/decibel.hpp>
#include <q/support/duration.hpp>
#include <q/support/literals.hpp>
#include <q/support/pitch_names.hpp>
#include <q/support/pitch.hpp>

//
// LVGL Support
//
#include "lvgl.h"
#include "esp_lvgl_port.h"

extern "C" { // because these files are C and not C++
    #include "lcd.h"
    #include "touch.h"
}

extern "C" const lv_font_t raleway_128;
extern "C" const lv_font_t fontawesome_48;

#define GEAR_SYMBOL "\xEF\x80\x93"

namespace q = cycfi::q;
using namespace q::literals;
using std::fixed;

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

UserSettings *userSettings = NULL;
bool is_ui_initialized = false;

typedef enum {
    SCREEN_STANDBY = 0,
    SCREEN_TUNING,
    SCREEN_MENU_TOP,
    SCREEN_MENU_TUNING,
} tuner_screen_t;

tuner_screen_t current_screen = SCREEN_STANDBY;
lv_obj_t *main_screen = NULL;

//
// ADC-Related
//
#define TUNER_ADC_UNIT                    ADC_UNIT_1
#define _TUNER_ADC_UNIT_STR(unit)         #unit
#define TUNER_ADC_UNIT_STR(unit)          _TUNER_ADC_UNIT_STR(unit)
#define TUNER_ADC_CONV_MODE               ADC_CONV_SINGLE_UNIT_1
#define TUNER_ADC_ATTEN                   ADC_ATTEN_DB_12
#define TUNER_ADC_BIT_WIDTH               12

// #if CONFIG_IDF_TARGET_ESP32 || CONFIG_IDF_TARGET_ESP32S2
#define TUNER_ADC_OUTPUT_TYPE             ADC_DIGI_OUTPUT_FORMAT_TYPE1
#define TUNER_ADC_GET_CHANNEL(p_data)     ((p_data)->type1.channel)
#define TUNER_ADC_GET_DATA(p_data)        ((p_data)->type1.data)
// #else
// #define TUNER_ADC_OUTPUT_TYPE             ADC_DIGI_OUTPUT_FORMAT_TYPE2
// #define TUNER_ADC_GET_CHANNEL(p_data)     ((p_data)->type2.channel)
// #define TUNER_ADC_GET_DATA(p_data)        ((p_data)->type2.data)
// #endif

// CYD @ 48kHz
#define TUNER_ADC_FRAME_SIZE            1024
#define TUNER_ADC_BUFFER_POOL_SIZE      4096
#define TUNER_ADC_SAMPLE_RATE           (48 * 1000) // 48kHz

#define WEIRD_ESP32_WROOM_32_FREQ_FIX_FACTOR    1.2222222223 // 11/9 but using 11/9 gives completely incorrect results. Weird.

// HELTEC @ 20kHz
// #define TUNER_ADC_FRAME_SIZE            (SOC_ADC_DIGI_DATA_BYTES_PER_CONV * 256)
// #define TUNER_ADC_BUFFER_POOL_SIZE      (TUNER_ADC_FRAME_SIZE * 16)
// #define TUNER_ADC_SAMPLE_RATE           (20 * 1000) // 20kHz

// Original HELTEC
// #define TUNER_ADC_FRAME_SIZE            (SOC_ADC_DIGI_DATA_BYTES_PER_CONV * 64)
// #define TUNER_ADC_BUFFER_POOL_SIZE      (TUNER_ADC_FRAME_SIZE * 4)
// #define TUNER_ADC_SAMPLE_RATE           (5 * 1000) // 5kHz

// If the difference between the minimum and maximum input values
// is less than this value, discard the reading and do not evaluate
// the frequency. This should help cut down on the noise from the
// OLED and only attempt to read frequency information when an
// actual input signal is being read.
#define TUNER_READING_DIFF_MINIMUM      400 // TODO: Convert this into a debug setting

//
// Smoothing
//

// 1EU Filter
#define EU_FILTER_ESTIMATED_FREQ        48000 // Same as https://github.com/bkshepherd/DaisySeedProjects/blob/main/Software/GuitarPedal/Util/frequency_detector_q.h
#define EU_FILTER_MIN_CUTOFF            0.5
#define EU_FILTER_DERIVATIVE_CUTOFF     1

#define A4_FREQ                         440.0
#define CENTS_PER_SEMITONE              100

#define INDICATOR_SEGMENTS              100 // num of visual segments for showing tuning accuracy
#define PITCH_INDICATOR_BAR_WIDTH       8

#define MAX_PITCH_NAME_LENGTH           4
using namespace cycfi::q::pitch_names;
using frequency = cycfi::q::frequency;
using pitch = cycfi::q::pitch;
CONSTEXPR frequency low_fs = cycfi::q::pitch_names::C[1];
CONSTEXPR frequency high_fs = cycfi::q::pitch_names::C[7]; // Helps to catch the high harmonics

static adc_channel_t channel[1] = {ADC_CHANNEL_7}; // ESP32-CYD - GPIO 35 (ADC1_CH7)
// static adc_channel_t channel[1] = {ADC_CHANNEL_4}; // ESP32 - Heltec

static TaskHandle_t s_task_handle;
static const char *TAG = "TUNER";

ExponentialSmoother smoother(0.1); // This is replaced by a user setting
float current_frequency = -1.0f;

// 1EU Filter Initialization Params
static const double euFilterFreq = EU_FILTER_ESTIMATED_FREQ; // I believe this means no guess as to what the incoming frequency will initially be
static const double mincutoff = EU_FILTER_MIN_CUTOFF;
static const double dcutoff = EU_FILTER_DERIVATIVE_CUTOFF;

OneEuroFilter oneEUFilter(euFilterFreq, mincutoff, 0.05, dcutoff); // TODO: Use the default beta instead of hard-coded one

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

static void oledTask(void *pvParameter);
static void readAndDetectTask(void *pvParameter);

static const char *note_names[] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
static const char *no_freq_name = "-";
lv_anim_t pitch_animation;
lv_coord_t last_pitch_indicator_pos = (lv_coord_t)0.0;
lv_timer_t *note_name_update_timer = NULL;

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

static bool IRAM_ATTR s_conv_done_cb(adc_continuous_handle_t handle, const adc_continuous_evt_data_t *edata, void *user_data)
{
    BaseType_t mustYield = pdFALSE;
    //Notify that ADC continuous driver has done enough number of conversions
    vTaskNotifyGiveFromISR(s_task_handle, &mustYield);

    return (mustYield == pdTRUE);
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
    // lv_style_set_text_font(&note_name_label_style, &lv_font_montserrat_48);
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
char noteNameBuffer[MAX_PITCH_NAME_LENGTH];

void set_note_name_cb(lv_timer_t * timer) {
    // The note name is in the timer's user data
    ESP_LOGI("LOCK", "locking in set_note_name_cb");
    lvgl_port_lock(0);
    const char *note_string = (const char *)lv_timer_get_user_data(timer);
    memset(noteNameBuffer, '\0', MAX_PITCH_NAME_LENGTH);
    snprintf(noteNameBuffer, MAX_PITCH_NAME_LENGTH, "%s", note_string);
    lv_label_set_text_static(note_name_label, (const char *)noteNameBuffer);

    lv_timer_pause(timer);

    ESP_LOGI("LOCK", "unlocking in set_note_name_cb");
    lvgl_port_unlock();
    ESP_LOGI("LOCK", "unlocked in set_note_name_cb");
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

static void continuous_adc_init(adc_channel_t *channel, uint8_t channel_count, adc_continuous_handle_t *out_handle)
{
    adc_continuous_handle_t handle = NULL;

    adc_continuous_handle_cfg_t adc_config = {
        .max_store_buf_size = TUNER_ADC_BUFFER_POOL_SIZE,
        .conv_frame_size = TUNER_ADC_FRAME_SIZE,
        .flags = {
            // .flush_pool = false,
            .flush_pool = true,
        },
    };
    ESP_ERROR_CHECK(adc_continuous_new_handle(&adc_config, &handle));

    adc_digi_pattern_config_t adc_pattern[SOC_ADC_PATT_LEN_MAX] = {0};
    for (int i = 0; i < channel_count; i++) {
        adc_pattern[i].atten = TUNER_ADC_ATTEN;
        adc_pattern[i].channel = channel[i] & 0x7;
        adc_pattern[i].unit = TUNER_ADC_UNIT;
        adc_pattern[i].bit_width = TUNER_ADC_BIT_WIDTH;

        ESP_LOGI(TAG, "adc_pattern[%d].atten is :%" PRIx8, i, adc_pattern[i].atten);
        ESP_LOGI(TAG, "adc_pattern[%d].channel is :%" PRIx8, i, adc_pattern[i].channel);
        ESP_LOGI(TAG, "adc_pattern[%d].unit is :%" PRIx8, i, adc_pattern[i].unit);
    }

    adc_continuous_config_t dig_cfg = {
        .pattern_num = channel_count,
        .adc_pattern = adc_pattern,
        .sample_freq_hz = TUNER_ADC_SAMPLE_RATE,
        .conv_mode = TUNER_ADC_CONV_MODE,
        .format = TUNER_ADC_OUTPUT_TYPE,
    };

    // dig_cfg.adc_pattern = adc_pattern;
    ESP_ERROR_CHECK(adc_continuous_config(handle, &dig_cfg));

    *out_handle = handle;
}

static esp_err_t app_lvgl_main() {
    ESP_LOGI("LOCK", "locking in app_lvgl_main");
    lvgl_port_lock(0);
    ESP_LOGI("LOCK", "locked in app_lvgl_main");

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

    ESP_LOGI("LOCK", "unlocking in app_lvgl_main");
    lvgl_port_unlock();
    ESP_LOGI("LOCK", "unlocked in app_lvgl_main");

    userSettings->setDisplayAndScreen(lvgl_display, main_screen);

    return ESP_OK;
}

void user_settings_changed() {
    ESP_LOGI(TAG, "User settings changed");
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

    oneEUFilter.setBeta(userSettings->oneEUBeta);
    smoother.setAmount(userSettings->expSmoothing);
}

extern "C" void app_main() {

    // Initialize NVS (Persistent Flash Storage for User Settings)
    userSettings = new UserSettings(user_settings_changed);

    // Start the Display Task
    xTaskCreatePinnedToCore(
        oledTask,           // callback function
        "oled",             // debug name of the task
        4096,               // stack depth (no idea what this should be)
        NULL,               // params to pass to the callback function
        0,                  // ux priority - higher value is higher priority
        NULL,               // handle to the created task - we don't need it
        0                   // Core ID - since we're not using Bluetooth/Wi-Fi, this can be 0 (the protocol CPU)
    );

    // Start the Pitch Reading & Detection Task
    xTaskCreatePinnedToCore(
        readAndDetectTask,  // callback function
        "detect",           // debug name of the task
        4096,               // stack depth (no idea what this should be)
        NULL,               // params to pass to the callback function
        1,                  // ux priority - higher value is higher priority
        NULL,               // handle to the created task - we don't need it
        1                   // Core ID
    );
}

static void oledTask(void *pvParameter) {
    // Prep the OLED
    // lv_disp_t *disp = NULL;
    // display_init(&disp);
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

    float cents;

    while(1) {
        lv_task_handler();

        // Lock the mutex due to the LVGL APIs are not thread-safe
        if (userSettings != NULL && !userSettings->isShowingMenu && lvgl_port_lock(0)) {
            // display_frequency(current_frequency);
            if (current_frequency > 0) {
                const char *noteName = get_pitch_name_and_cents_from_frequency(current_frequency, &cents);
                // ESP_LOGI(TAG, "%s - %d", noteName, cents);
                display_pitch(current_frequency, noteName, cents);
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

static void readAndDetectTask(void *pvParameter) {
    // Prep ADC
    esp_err_t ret;
    uint32_t num_of_bytes_read = 0;
    uint8_t *adc_buffer = (uint8_t *)malloc(TUNER_ADC_FRAME_SIZE);
    if (adc_buffer == NULL) {
        ESP_LOGI(TAG, "Failed to allocate memory for buffer");
        return;
    }
    memset(adc_buffer, 0xcc, TUNER_ADC_FRAME_SIZE);

    // Get the pitch detector ready
    q::pitch_detector   pd(low_fs, high_fs, TUNER_ADC_SAMPLE_RATE, -40_dB);

    // Use `lastFrequencyRecordedTime` to know elapsed time since
    // the frequency was updated for the UI to read. Don't update
    // more frequently than `minIntervalForFrequencyUpdate`.
    int64_t lastFrequencyRecordedTime = esp_timer_get_time();

    // Don't update the UI more frequently than this interval
    int64_t minIntervalForFrequencyUpdate = 50 * 1000; // 50ms

    // auto const&                 bits = pd.bits();
    // auto const&                 edges = pd.edges();
    // q::bitstream_acf<>          bacf{ bits };
    // auto                        min_period = as_float(high_fs.period()) * TUNER_ADC_SAMPLE_RATE;
    
    // q::peak_envelope_follower   env{ 30_ms, TUNER_ADC_SAMPLE_RATE };
    // q::one_pole_lowpass         lp{high_fs, TUNER_ADC_SAMPLE_RATE};
    // q::one_pole_lowpass         lp2(low_fs, TUNER_ADC_SAMPLE_RATE);

    // constexpr float             slope = 1.0f/2;
    // constexpr float             makeup_gain = 2;
    // q::compressor               comp{ -18_dB, slope };
    // q::clip                     clip;

    // float                       onset_threshold = lin_float(-28_dB);
    // float                       release_threshold = lin_float(-60_dB);
    // float                       threshold = onset_threshold;

    auto sc_conf = q::signal_conditioner::config{};
    auto sig_cond = q::signal_conditioner{sc_conf, low_fs, high_fs, TUNER_ADC_SAMPLE_RATE};

    s_task_handle = xTaskGetCurrentTaskHandle();
    
    adc_continuous_handle_t handle = NULL;
    continuous_adc_init(channel, sizeof(channel) / sizeof(adc_channel_t), &handle);

    adc_continuous_evt_cbs_t cbs = {
        .on_conv_done = s_conv_done_cb,
    };
    ESP_ERROR_CHECK(adc_continuous_register_event_callbacks(handle, &cbs, NULL));
    ESP_ERROR_CHECK(adc_continuous_start(handle));

    // adc_ll_digi_set_convert_limit_num(2); // potential hack for the ESP32 ADC bug

    while (1) {
        /**
         * This is to show you the way to use the ADC continuous mode driver event callback.
         * This `ulTaskNotifyTake` will block when the data processing in the task is fast.
         * However in this example, the data processing (print) is slow, so you barely block here.
         *
         * Without using this event callback (to notify this task), you can still just call
         * `adc_continuous_read()` here in a loop, with/without a certain block timeout.
         */
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

        while (1) {
            if (userSettings == NULL || userSettings->isShowingMenu) {
                // Don't read the signal when the settings menu is showing.
                vTaskDelay(pdMS_TO_TICKS(500));
                continue;
            }

            std::vector<float> in(TUNER_ADC_FRAME_SIZE); // a vector of values to pass into qlib

            ret = adc_continuous_read(handle, adc_buffer, TUNER_ADC_FRAME_SIZE, &num_of_bytes_read, portMAX_DELAY);
            if (ret == ESP_OK) {
                // ESP_LOGI(TAG, "ret is %x, num_of_bytes_read is %"PRIu32" bytes", ret, num_of_bytes_read);

                // Get the data out of the ADC Conversion Result.
                int valuesStored = 0;
                float maxVal = 0;
                float minVal = MAXFLOAT;
                // ESP_LOGI(TAG, "Bytes read: %ld", num_of_bytes_read);
                for (int i = 0; i < num_of_bytes_read; i += SOC_ADC_DIGI_RESULT_BYTES, valuesStored++) {
                    adc_digi_output_data_t *p = (adc_digi_output_data_t*)&adc_buffer[i];
                    // if (i == 20) {
                    //     ESP_LOGI(TAG, "read value is: %d", TUNER_ADC_GET_DATA(p));
                    // }

                    // Do a first pass by just storing the raw values into the float array
                    in[valuesStored] = TUNER_ADC_GET_DATA(p);

                    // Track the min and max values we see so we can convert to values between -1.0f and +1.0f
                    if (in[valuesStored] > maxVal) {
                        maxVal = in[valuesStored];
                    }
                    if (in[valuesStored] < minVal) {
                        minVal = in[valuesStored];
                    }
                }

                // Bail out if the input does not meet the minimum criteria
                float range = maxVal - minVal;
                if (range < TUNER_READING_DIFF_MINIMUM) {
                    current_frequency = -1; // Indicate to the UI that there's no frequency available
                    oneEUFilter.reset(); // Reset the 1EU filter so the next frequency it detects will be as fast as possible
                    smoother.reset();
                    pd.reset();
                    vTaskDelay(10 / portTICK_PERIOD_MS); // Should be 10ms?
                    continue;
                }

                // ESP_LOGI(TAG, "Min: %f, Max: %f, peak-to-peak: %f", minVal, maxVal, range);

                // Normalize the values between -1.0 and +1.0 before processing with qlib.
                float midVal = range / 2;
                // ESP_LOGI("min, max, range, mid:", "%f, %f, %f, %f", minVal, maxVal, range, midVal);
                for (auto i = 0; i < valuesStored; i++) {
                    // ESP_LOGI("adc", "%f", in[i]);
                    float newPosition = in[i] - midVal - minVal;
                    float normalizedValue = newPosition / midVal;
                    // ESP_LOGI("norm-val", "%f is now %f", in[i], normalizedValue);
                    in[i] = normalizedValue;

                    auto s = in[i]; // input signal

                    // I've got the signal conditioning commented out right now
                    // because it actually is making the frequency readings
                    // NOT work. They probably just need to be tweaked a little.

                    // Signal Conditioner
                    // s = sig_cond(s);

                    // // Bandpass filter
                    // s = lp(s);
                    // s -= lp2(s);

                    // // Envelope
                    // auto e = env(std::abs(static_cast<int>(s)));
                    // auto e_db = q::lin_to_db(e);

                    // if (e > threshold) {
                    //     // Compressor + make-up gain + hard clip
                    //     auto gain = cycfi::q::lin_float(comp(e_db)) * makeup_gain;
                    //     s = clip(s * gain);
                    //     threshold = release_threshold;
                    // } else {
                    //     s = 0.0f;
                    //     threshold = onset_threshold;
                    // }

                    int64_t time_us = esp_timer_get_time(); // Get time in microseconds
                    int64_t time_ms = time_us / 1000; // Convert to milliseconds
                    int64_t time_seconds = time_us / 1000000;    // Convert to seconds
                    // int64_t elapsedTimeSinceLastUpdate = time_us - lastFrequencyRecordedTime;

                    // Pitch Detect
                    // Send in each value into the pitch detector
                    // if (pd(s) == true && elapsedTimeSinceLastUpdate >= minIntervalForFrequencyUpdate) { // calculated a frequency
                    if (pd(s) == true) { // calculated a frequency
                        auto f = pd.get_frequency();

                        bool use1EUFilterFirst = false; // TODO: This may never be needed. Need to test which "feels" better for tuning
                        if (use1EUFilterFirst) {
                            // 1EU Filtering
                            f = (float)oneEUFilter.filter((double)f, (TimeStamp)time_seconds); // Stores the current frequency in the global current_frequency variable

                            // Simple Exponential Smoothing
                            f = smoother.smooth(f);
                        } else {
                            // Simple Expoential Smoothing
                            f = smoother.smooth(f);

                            // 1EU Filtering
                            f = (float)oneEUFilter.filter((double)f, (TimeStamp)time_seconds); // Stores the current frequency in the global current_frequency variable
                        }

                        current_frequency = f / WEIRD_ESP32_WROOM_32_FREQ_FIX_FACTOR;
                    }
                }

                /**
                 * Because printing is slow, so every time you call `ulTaskNotifyTake`, it will immediately return.
                 * To avoid a task watchdog timeout, add a delay here. When you replace the way you process the data,
                 * usually you don't need this delay (as this task will block for a while).
                 */
                // vTaskDelay(1); // potentially no longer needed
                vTaskDelay(10 / portTICK_PERIOD_MS); // Should be 10ms?
            } else if (ret == ESP_ERR_TIMEOUT) {
                //We try to read `EXAMPLE_READ_LEN` until API returns timeout, which means there's no available data
                break;
            }
        }
    }

    free(adc_buffer);

    ESP_ERROR_CHECK(adc_continuous_stop(handle));
    ESP_ERROR_CHECK(adc_continuous_deinit(handle));
}