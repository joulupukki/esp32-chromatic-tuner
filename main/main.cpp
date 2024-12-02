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

namespace q = cycfi::q;
using namespace q::literals;
using std::fixed;

//
// ADC-Related
//
#define TUNER_ADC_UNIT                    ADC_UNIT_1
#define _TUNER_ADC_UNIT_STR(unit)         #unit
#define TUNER_ADC_UNIT_STR(unit)          _TUNER_ADC_UNIT_STR(unit)
#define TUNER_ADC_CONV_MODE               ADC_CONV_SINGLE_UNIT_1
#define TUNER_ADC_ATTEN                   ADC_ATTEN_DB_12
// #define TUNER_ADC_BIT_WIDTH               SOC_ADC_DIGI_MAX_BITWIDTH
#define TUNER_ADC_BIT_WIDTH               12

#if CONFIG_IDF_TARGET_ESP32 || CONFIG_IDF_TARGET_ESP32S2
#define TUNER_ADC_OUTPUT_TYPE             ADC_DIGI_OUTPUT_FORMAT_TYPE1
#define TUNER_ADC_GET_CHANNEL(p_data)     ((p_data)->type1.channel)
#define TUNER_ADC_GET_DATA(p_data)        ((p_data)->type1.data)
#else
#define TUNER_ADC_OUTPUT_TYPE             ADC_DIGI_OUTPUT_FORMAT_TYPE2
#define TUNER_ADC_GET_CHANNEL(p_data)     ((p_data)->type2.channel)
#define TUNER_ADC_GET_DATA(p_data)        ((p_data)->type2.data)
#endif

// CYD @ 48kHz
#define TUNER_ADC_FRAME_SIZE            1024
#define TUNER_ADC_BUFFER_POOL_SIZE      4096
#define TUNER_ADC_SAMPLE_RATE           (48 * 1000) // 48kHz

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
#define TUNER_READING_DIFF_MINIMUM      200

//
// Smoothing
//

// 1EU Filter
#define EU_FILTER_ESTIMATED_FREQ        48000 // Same as https://github.com/bkshepherd/DaisySeedProjects/blob/main/Software/GuitarPedal/Util/frequency_detector_q.h
#define EU_FILTER_MIN_CUTOFF            0.5
// #define EU_FILTER_BETA                  0.007
#define EU_FILTER_BETA                  0.05 // Same as https://github.com/bkshepherd/DaisySeedProjects/blob/main/Software/GuitarPedal/Util/frequency_detector_q.h
#define EU_FILTER_DERIVATIVE_CUTOFF     1

// Exponential Smoother (not currently used)
#define SMOOTHING_AMOUNT                0.25 // should be a value between 0.0 and 1.0

//
// Pitch Name & Display
//
#define A4_FREQ                         440.0
#define CENTS_PER_SEMITONE              100

#define INDICATOR_SEGMENTS              17 // num of visual segments
#define IN_TUNE_CENTS_WIDTH             4 // num of cents around the 0 point considered as "in tune"
#define PITCH_INDICATOR_BAR_WIDTH       8

#define MAX_PITCH_NAME_LENGTH           8

using namespace cycfi::q::pitch_names;
using frequency = cycfi::q::frequency;
using pitch = cycfi::q::pitch;
CONSTEXPR frequency low_fs = cycfi::q::pitch_names::C[1];
CONSTEXPR frequency high_fs = cycfi::q::pitch_names::C[5];

static adc_channel_t channel[1] = {ADC_CHANNEL_7}; // ESP32-CYD
// static adc_channel_t channel[1] = {ADC_CHANNEL_4}; // ESP32 - Heltec

static TaskHandle_t s_task_handle;
static const char *TAG = "TUNER";

ExponentialSmoother smoother(SMOOTHING_AMOUNT);
float current_frequency = -1.0f;

// 1EU Filter Initialization Params
static const double euFilterFreq = EU_FILTER_ESTIMATED_FREQ; // I believe this means no guess as to what the incoming frequency will initially be
static const double mincutoff = EU_FILTER_MIN_CUTOFF;
static const double beta = EU_FILTER_BETA;
static const double dcutoff = EU_FILTER_DERIVATIVE_CUTOFF;

OneEuroFilter oneEUFilter(euFilterFreq, mincutoff, beta, dcutoff) ;

lv_obj_t *frequency_label;
lv_style_t frequency_label_style;
lv_obj_t *cents_label;
lv_style_t cents_label_style;

lv_obj_t *pitch_indicator_bar;
lv_obj_t *pitch_target_bar_top;
lv_coord_t screen_width = 0;
lv_coord_t screen_height = 0;

static void oledTask(void *pvParameter);
static void readAndDetectTask(void *pvParameter);

static const char *note_names[] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
static const char *no_freq_name = "-";

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
    // snprintf(pitch_name, MAX_PITCH_NAME_LENGTH, "%s", note_names[note_index]);
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

void create_labels() {
    lv_obj_t *scr = lv_scr_act();

    // Frequency Label (the big font at the bottom middle)
    frequency_label = lv_label_create(scr);
    lv_label_set_long_mode(frequency_label, LV_LABEL_LONG_CLIP);

    lv_label_set_text_static(frequency_label, no_freq_name);
    lv_obj_set_width(frequency_label, lv_obj_get_width(scr));
    lv_obj_set_style_text_align(frequency_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(frequency_label, LV_ALIGN_BOTTOM_MID, 0, 0);

    lv_style_init(&frequency_label_style);
    lv_style_set_text_font(&frequency_label_style, &lv_font_montserrat_48);
    lv_obj_add_style(frequency_label, &frequency_label_style, 0);

    // Cents Label (shown right under the pitch indicator bar)
    cents_label = lv_label_create(scr);
    
    lv_style_init(&cents_label_style);
    lv_style_set_text_font(&cents_label_style, &lv_font_montserrat_14);
    lv_obj_add_style(cents_label, &cents_label_style, 0);

    lv_obj_set_width(cents_label, lv_obj_get_width(scr) / 2);
    lv_obj_set_style_text_align(cents_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(cents_label, LV_ALIGN_CENTER, 0, 0);
    lv_obj_add_flag(cents_label, LV_OBJ_FLAG_HIDDEN);
}

void create_indicators() {
    lv_obj_t *scr = lv_scr_act();
    screen_width = lv_obj_get_width(scr);
    screen_height = lv_obj_get_height(scr);

    //
    // Create the indicator bar. This is the bar
    // that moves around during tuning.
    lv_obj_t * rect = lv_obj_create(scr);

    // Set the rectangle's size and position
    lv_obj_set_size(rect, PITCH_INDICATOR_BAR_WIDTH, screen_height / 3);
    lv_obj_align(rect, LV_ALIGN_TOP_MID, 0, 0);

    // Set the rectangle's style (optional)
    lv_obj_set_style_bg_color(rect, lv_color_black(), LV_PART_MAIN);

    pitch_indicator_bar = rect;

    //
    // Create the target bar. This is the bar
    // that stays put and indicates where tuning
    // is centered.
    rect = lv_obj_create(scr);
    lv_obj_set_size(rect, 6, screen_height / 6);
    lv_obj_align(rect, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_bg_color(rect, lv_color_black(), LV_PART_MAIN);
    pitch_target_bar_top = rect;

    // Hide these bars initially
    lv_obj_add_flag(pitch_indicator_bar, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(pitch_target_bar_top, LV_OBJ_FLAG_HIDDEN);
}

// This function is for debug and will just show the frequency and nothing else
void display_frequency(float frequency) {
    lv_label_set_text_fmt(frequency_label, "Freq: %f", frequency);
}

const char *lastDisplayedNote = no_freq_name;
char noteNameBuffer[8];

void display_pitch(const char *noteName, float cents) {
    if (noteName != NULL) {
        // Show a noteName with indicators
        if (lastDisplayedNote != noteName) {
            sprintf(noteNameBuffer, "%s", noteName);
            lv_label_set_text_static(frequency_label, noteNameBuffer); // need to use because of ADC (being changed so frequently)
            lastDisplayedNote = noteName; // prevent setting this so often to help prevent an LVGL crash
        }

        // Calculate where the indicator bar should be left-to right based on the cents
        lv_coord_t indicator_x_pos = (lv_coord_t)0.0;
        // auto cents_per_side = CENTS_PER_SEMITONE / 2;
        // lv_coord_t half_width = screen_width / 2;
        if (abs(cents) <= IN_TUNE_CENTS_WIDTH / 2) {
            // Show this as perfectly in tune
            indicator_x_pos = 0;
        } else {
            float segment_width_cents = CENTS_PER_SEMITONE / INDICATOR_SEGMENTS; // TODO: Make this a static var
            int segment_index = cents / segment_width_cents;

            float segment_width_pixels = screen_width / INDICATOR_SEGMENTS;
            indicator_x_pos = segment_index * segment_width_pixels; 
        }

        lv_obj_align(pitch_indicator_bar, LV_ALIGN_TOP_MID, indicator_x_pos, 18);

        // Make the two bars show up
        lv_obj_clear_flag(pitch_indicator_bar, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(pitch_target_bar_top, LV_OBJ_FLAG_HIDDEN);

        lv_label_set_text_fmt(cents_label, "%.1f", cents);
        lv_obj_align(cents_label, LV_ALIGN_CENTER, indicator_x_pos, 4);
        lv_obj_clear_flag(cents_label, LV_OBJ_FLAG_HIDDEN);
    } else {
        // Hide the pitch and indicators since it's not detected
        if (lastDisplayedNote != no_freq_name) {
            sprintf(noteNameBuffer, "%s", no_freq_name);
            lv_label_set_text_static(frequency_label, noteNameBuffer);
            lastDisplayedNote = no_freq_name;
        }

        // Hide the indicator bars and cents label
        lv_obj_add_flag(pitch_indicator_bar, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(pitch_target_bar_top, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(cents_label, LV_OBJ_FLAG_HIDDEN);
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

// static void display_init(lv_disp_t **out_handle) {

//     // Configure I2C as master
//     i2c_master_bus_config_t i2c_mst_config = {
//         .i2c_port = 0,
//         .sda_io_num = SDA_OLED,
//         .scl_io_num = SCL_OLED,
//         .clk_source = I2C_CLK_SRC_DEFAULT,
//         .glitch_ignore_cnt = 7,
//         // .intr_priority = 1,
//         // .trans_queue_depth = 10,
//         .flags = {
//             .enable_internal_pullup = true // Internal pull-up resistor.
//         }
//     };

//     i2c_master_bus_handle_t bus_handle;
//     ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_mst_config, &bus_handle));

//     // Configure the OLED
//     esp_lcd_panel_io_handle_t io_handle = NULL;
//     esp_lcd_panel_io_i2c_config_t io_config = {
//         .dev_addr = 0x3c,
//         .control_phase_bytes = 1, // refer to LCD spec
//         .dc_bit_offset = 6,       // refer to LCD spec
//         .lcd_cmd_bits = LCD_CMD_BITS,
//         .lcd_param_bits = LCD_CMD_BITS,
//         .scl_speed_hz = LCD_PIXEL_CLOCK_HZ
//     };
//     ESP_ERROR_CHECK(esp_lcd_new_panel_io_i2c_v2(bus_handle, &io_config, &io_handle));

//     esp_lcd_panel_dev_config_t panel_config = {
//         .reset_gpio_num = RST_OLED,
//         .bits_per_pixel = 1,
//     };
//     esp_lcd_panel_ssd1306_config_t ssd1306_config = {
//         .height = 64
//     };
//     panel_config.vendor_config = &ssd1306_config;

//     esp_lcd_panel_handle_t panel_handle = NULL;
//     ESP_ERROR_CHECK(esp_lcd_new_panel_ssd1306(io_handle, &panel_config, &panel_handle));

//     ESP_ERROR_CHECK(esp_lcd_panel_reset(panel_handle));
//     ESP_ERROR_CHECK(esp_lcd_panel_init(panel_handle));
//     ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel_handle, true));

//     ESP_LOGI(TAG, "Initialize LVGL");
//     const lvgl_port_cfg_t lvgl_cfg = ESP_LVGL_PORT_INIT_CONFIG();
//     lvgl_port_init(&lvgl_cfg);

//     const lvgl_port_display_cfg_t disp_cfg = {
//         .io_handle = io_handle,
//         .panel_handle = panel_handle,
//         .buffer_size = LCD_H_RES * LCD_V_RES,
//         .double_buffer = true,
//         .hres = LCD_H_RES,
//         .vres = LCD_V_RES,
//         .monochrome = true,
//         .rotation = {
//             .swap_xy = false,
//             .mirror_x = false,
//             .mirror_y = false,
//         },
//         .flags = {
//             .buff_dma = true,
//             .sw_rotate = false
//         }
//     };

//     lv_disp_t *disp = lvgl_port_add_disp(&disp_cfg);

//     // Rotate the screen so it will work inside of the 125B enclosure (portrait mode with the USB input pointing up)
//     lv_disp_set_rotation(disp, LV_DISP_ROT_270);

//     create_labels(disp);
//     create_indicators(disp);
    
//     *out_handle = disp;
// }

static esp_err_t app_lvgl_main()
{
    lvgl_port_lock(0);

    create_labels();
    create_indicators();

    // lv_obj_t *label = lv_label_create(scr);
    // lv_label_set_text(label, "Hello LVGL 9 and esp_lvgl_port!");
    // lv_obj_set_style_text_color(label, lv_color_white(), LV_STATE_DEFAULT);
    // lv_obj_align(label, LV_ALIGN_BOTTOM_MID, 0, -48);

    // lv_obj_t *labelR = lv_label_create(scr);
    // lv_label_set_text(labelR, "Red");
    // lv_obj_set_style_text_color(labelR, lv_color_make(0xff, 0, 0), LV_STATE_DEFAULT);
    // lv_obj_align(labelR, LV_ALIGN_TOP_MID, 0, 0);

    // lv_obj_t *labelG = lv_label_create(scr);
    // lv_label_set_text(labelG, "Green");
    // lv_obj_set_style_text_color(labelG, lv_color_make(0, 0xff, 0), LV_STATE_DEFAULT);
    // lv_obj_align(labelG, LV_ALIGN_TOP_MID, 0, 32);

    // lv_obj_t *labelB = lv_label_create(scr);
    // lv_label_set_text(labelB, "Blue");
    // lv_obj_set_style_text_color(labelB, lv_color_make(0, 0, 0xff), LV_STATE_DEFAULT);
    // lv_obj_align(labelB, LV_ALIGN_TOP_MID, 0, 64);

    // lv_obj_t *btn_counter = lv_button_create(scr);
    // lv_obj_align(btn_counter, LV_ALIGN_CENTER, 0, 0);
    // lv_obj_set_size(btn_counter, 120, 50);
    // lv_obj_add_event_cb(btn_counter, ui_event_Screen, LV_EVENT_ALL, btn_counter);

    // lbl_counter = lv_label_create(btn_counter);
    // lv_label_set_text(lbl_counter, "testing");
    // lv_obj_set_style_text_color(lbl_counter, lv_color_make(248, 11, 181), LV_STATE_DEFAULT);
    // lv_obj_align(lbl_counter, LV_ALIGN_CENTER, 0, 0);

    lvgl_port_unlock();

    return ESP_OK;
}


extern "C" void app_main() {
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
        10,                 // ux priority - higher value is higher priority
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
    lv_display_t *lvgl_display = NULL;

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

    ESP_ERROR_CHECK(lcd_display_brightness_set(75));
    ESP_ERROR_CHECK(lcd_display_rotate(lvgl_display, LV_DISPLAY_ROTATION_90));
    ESP_ERROR_CHECK(app_lvgl_main());

    float cents;

    while(1) {
        lv_task_handler();

        // Lock the mutex due to the LVGL APIs are not thread-safe
        if (lvgl_port_lock(0)) {
            // display_frequency(current_frequency);
            if (current_frequency > 0) {
                const char *noteName = get_pitch_name_and_cents_from_frequency(current_frequency, &cents);
                // ESP_LOGI(TAG, "%s - %d", noteName, cents);
                display_pitch(noteName, cents);
                // display_frequency(current_frequency);
            } else {
                display_pitch(NULL, 0);
            }
            // Release the mutex
            lvgl_port_unlock();
        }

        // vTaskDelay(pdMS_TO_TICKS(1)); // ~33 times per second
        vTaskDelay(pdMS_TO_TICKS(100)); // Yields to reset watchdog
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
                        auto rawF = pd.get_frequency();

                        // Simple Expoential Smoothing
                        auto expF = smoother.smooth(rawF);

                        // 1EU Filtering
                        auto oneEuF = (float)oneEUFilter.filter((double)expF, (TimeStamp)time_seconds); // Stores the current frequency in the global current_frequency variable
                        // current_frequency = oneEuF / (11/9); // approx 1.2222
                        // current_frequency = oneEuF / (1/.8181); // approx 1.2222
                        // auto fixFactor = 1/.81818181;
                        auto fixFactor = 1.2222222223;
                        current_frequency = oneEuF / fixFactor; // approx 1.2222
                        // current_frequency = oneEuF;

                        // lastFrequencyRecordedTime = time_us;

                        // ESP_LOGI("QLib", "Range: %d, Raw: %f, ExpF: %f, 1EU: %f, corrected: %f", (int)range, rawF, expF, oneEuF, current_frequency);
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