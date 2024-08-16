/*=============================================================================
    Copyright (c) 2024 Boyd Timothy. All rights reserved.

    Distributed under the MIT License [ https://opensource.org/licenses/MIT ]
=============================================================================*/
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "esp_adc/adc_continuous.h"
#include "exponential_smoother.hpp"

//
// Q DSP Library for Pitch Detection
//
#include <q/pitch/pitch_detector.hpp>
#include <q/fx/dynamic.hpp>
#include <q/fx/clip.hpp>
#include <q/support/decibel.hpp>
#include <q/support/duration.hpp>
#include <q/support/literals.hpp>
#include <q/support/pitch_names.hpp>
#include <q/support/pitch.hpp>

//
// OLED Support
//
#include "pins_heltec.h" // taken from the Heltec Library to get the pins needed to power and control the OLED
#include "esp_lcd_panel_vendor.h"
#include "driver/i2c_master.h"
#include "esp_lvgl_port.h"

#define TUNER_ADC_UNIT                    ADC_UNIT_1
#define _TUNER_ADC_UNIT_STR(unit)         #unit
#define TUNER_ADC_UNIT_STR(unit)          _TUNER_ADC_UNIT_STR(unit)
#define TUNER_ADC_CONV_MODE               ADC_CONV_SINGLE_UNIT_1
// #define TUNER_ADC_CONV_MODE               ADC_CONV_BOTH_UNIT
#define TUNER_ADC_ATTEN                   ADC_ATTEN_DB_12
#define TUNER_ADC_BIT_WIDTH               SOC_ADC_DIGI_MAX_BITWIDTH

#if CONFIG_IDF_TARGET_ESP32 || CONFIG_IDF_TARGET_ESP32S2
#define TUNER_ADC_OUTPUT_TYPE             ADC_DIGI_OUTPUT_FORMAT_TYPE1
#define TUNER_ADC_GET_CHANNEL(p_data)     ((p_data)->type1.channel)
#define TUNER_ADC_GET_DATA(p_data)        ((p_data)->type1.data)
#else
#define TUNER_ADC_OUTPUT_TYPE             ADC_DIGI_OUTPUT_FORMAT_TYPE2
#define TUNER_ADC_GET_CHANNEL(p_data)     ((p_data)->type2.channel)
#define TUNER_ADC_GET_DATA(p_data)        ((p_data)->type2.data)
#endif

#define SMOOTHING_AMOUNT           0.25 // should be a value between 0.0 and 1.0
// #define TUNER_ADC_BUFFER_POOL_SIZE    2048 * SOC_ADC_DIGI_DATA_BYTES_PER_CONV
// #define TUNER_ADC_FRAME_SIZE  512 * SOC_ADC_DIGI_DATA_BYTES_PER_CONV
// #define TUNER_ADC_BUFFER_POOL_SIZE      2048
// #define TUNER_ADC_FRAME_SIZE            512
#define TUNER_ADC_BUFFER_POOL_SIZE      1024
#define TUNER_ADC_FRAME_SIZE            256

// The minimum threshold sample frequency is 20 * 1000 according
// to esp-idf/components/soc/esp32/include/soc/soc_caps.h.
// As far as I understand, this is the frequency sampling
// rate (Samples Per Second/sps).
// #define TUNER_ADC_SAMPLE_RATE 20 * 1000
// #define TUNER_ADC_SAMPLE_RATE 10000
// #define TUNER_ADC_SAMPLE_RATE 5000
#define TUNER_ADC_SAMPLE_RATE 5000

// If the difference between the minimum and maximum input values
// is less than this value, discard the reading and do not evaluate
// the frequency. This should help cut down on the noise from the
// OLED and only attempt to read frequency information when an
// actual input signal is being read.
#define TUNER_READING_DIFF_MINIMUM 100

#define A4_FREQ 440.0
#define CENTS_PER_SEMITONE 100

//
// OLED Items
//
#define LCD_PIXEL_CLOCK_HZ      500000
// Bit number used to represent command and parameter
#define LCD_CMD_BITS    8
#define LCD_PARAM_BITS  8
#define LCD_H_RES       128
#define LCD_V_RES       64

using namespace cycfi::q::pitch_names;
using frequency = cycfi::q::frequency;
using pitch = cycfi::q::pitch;
CONSTEXPR frequency low_fs  = frequency(27.5);
CONSTEXPR frequency high_fs = frequency(1000.0);

#if CONFIG_IDF_TARGET_ESP32
static adc_channel_t channel[1] = {ADC_CHANNEL_4};
#else
static adc_channel_t channel[1] = {ADC_CHANNEL_1};
#endif

namespace q = cycfi::q;
using namespace q::literals;
using std::fixed;

static TaskHandle_t s_task_handle;
static const char *TAG = "TUNER";

ExponentialSmoother smoother(SMOOTHING_AMOUNT);
float current_frequency = -1.0f;

lv_obj_t *frequency_label;
lv_obj_t *pitch_indicator_bar;
lv_obj_t *pitch_target_bar_top;
lv_obj_t *pitch_target_bar_bottom;
lv_coord_t screen_width = 0;
lv_coord_t screen_height = 0;

static void oledTask(void *pvParameter);
static void readAndDetectTask(void *pvParameter);

// Function to calculate the MIDI note number from frequency
double midi_note_from_frequency(double freq) {
    return 69 + 12 * log2(freq / A4_FREQ);
}

// Function to get pitch name and cents from MIDI note number
void get_pitch_name_and_cents_from_frequency(float freq, char *pitch_name, int *cents) {
    double midi_note = midi_note_from_frequency(freq);
    int octave = (int)(midi_note / 12) - 1;
    int note_index = (int)fmod(midi_note, 12);
    double fractional_note = midi_note - (int)midi_note;

    static const char *note_names[] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};

    *cents = (int)(fractional_note * CENTS_PER_SEMITONE);
    if (*cents >= CENTS_PER_SEMITONE / 2) {
        // Adjust the note index so that it still shows the proper note
        note_index++;
        if (note_index >= 12) {
            note_index = 0; // Overflow to the beginning of the note names
        }
        *cents = *cents - CENTS_PER_SEMITONE;
    }

    strncpy(pitch_name, note_names[note_index], strlen(note_names[note_index]));
    pitch_name[strlen(note_names[note_index])] = '\0';
}

static bool IRAM_ATTR s_conv_done_cb(adc_continuous_handle_t handle, const adc_continuous_evt_data_t *edata, void *user_data)
{
    BaseType_t mustYield = pdFALSE;
    //Notify that ADC continuous driver has done enough number of conversions
    vTaskNotifyGiveFromISR(s_task_handle, &mustYield);

    return (mustYield == pdTRUE);
}

void create_frequency_label(lv_disp_t *disp) {
    lv_obj_t *scr = lv_disp_get_scr_act(disp);
    frequency_label = lv_label_create(scr);
    // lv_label_set_long_mode(frequency_label, LV_LABEL_LONG_CLIP);
    lv_label_set_long_mode(frequency_label, LV_LABEL_LONG_CLIP);
    lv_label_set_text(frequency_label, "-");
    /* Size of the screen (if you use rotation 90 or 270, please set disp->driver->ver_res) */
    lv_obj_set_width(frequency_label, disp->driver->hor_res);
    // lv_obj_align(frequency_label, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_text_align(frequency_label, LV_TEXT_ALIGN_CENTER, 0);
    // lv_obj_align(frequency_label, LV_ALIGN_CENTER, 0, 0);
    lv_obj_align(frequency_label, LV_ALIGN_BOTTOM_MID, 0, 0);
    // lv_obj_align( lbl, NULL, LV_ALIGN_CENTER, 0, 70 );
    // lv_obj_set_auto_realign(frequency_label, true);
}

void create_indicators(lv_disp_t *disp) {
    lv_obj_t *scr = lv_disp_get_scr_act(disp);
    screen_width = lv_obj_get_width(scr);
    screen_height = lv_obj_get_height(scr);

    //
    // Create the indicator bar. This is the bar
    // that moves around during tuning.
    lv_obj_t * rect = lv_obj_create(scr);

    // Set the rectangle's size and position
    lv_obj_set_size(rect, 6, screen_height / 3);
    lv_obj_align(rect, LV_ALIGN_TOP_MID, 0, 0);
    // lv_obj_align(rect, LV_ALIGN_CENTER, 0, 0);

    // Set the rectangle's style (optional)
    lv_obj_set_style_bg_color(rect, lv_color_white(), LV_PART_MAIN);

    pitch_indicator_bar = rect;

    //
    // Create the target bar. This is the bar
    // that stays put and indicates where tuning
    // is centered.
    rect = lv_obj_create(scr);
    lv_obj_set_size(rect, 6, screen_height / 6);
    lv_obj_align(rect, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_bg_color(rect, lv_color_white(), LV_PART_MAIN);
    pitch_target_bar_top = rect;

    rect = lv_obj_create(scr);
    lv_obj_set_size(rect, 6, screen_height / 6);
    lv_obj_align(rect, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_color(rect, lv_color_white(), LV_PART_MAIN);
    pitch_target_bar_bottom = rect;
}

void display_frequency(float frequency) {
    lv_label_set_text_fmt(frequency_label, "Freq: %f", frequency);
}

void display_pitch(char *pitch, int cents) {
    if (strlen(pitch) > 0) {
        lv_label_set_text_fmt(frequency_label, "%s %d", pitch, cents);

        // Calculate where the indicator bar should be left-to right based on the cents

        auto cents_per_side = CENTS_PER_SEMITONE / 2;
        lv_coord_t half_width = screen_width / 2;
        float cents_percentage = (float)cents / (float)cents_per_side;
        lv_coord_t indicator_x_pos = (lv_coord_t)(half_width * cents_percentage);

        lv_obj_align(pitch_indicator_bar, LV_ALIGN_TOP_MID, indicator_x_pos, 0);

        // Make the two bars show up
        lv_obj_set_style_bg_color(pitch_indicator_bar, lv_color_black(), LV_PART_MAIN);
        lv_obj_set_style_bg_color(pitch_target_bar_top, lv_color_black(), LV_PART_MAIN);
        lv_obj_set_style_bg_color(pitch_target_bar_bottom, lv_color_black(), LV_PART_MAIN);
    } else {
        lv_label_set_text(frequency_label, "-");

        // Make the two bars hide themselves
        lv_obj_set_style_bg_color(pitch_indicator_bar, lv_color_white(), LV_PART_MAIN);
        lv_obj_set_style_bg_color(pitch_target_bar_top, lv_color_white(), LV_PART_MAIN);
        lv_obj_set_style_bg_color(pitch_target_bar_bottom, lv_color_white(), LV_PART_MAIN);
    }
}

static void continuous_adc_init(adc_channel_t *channel, uint8_t channel_num, adc_continuous_handle_t *out_handle)
{
    adc_continuous_handle_t handle = NULL;

    adc_continuous_handle_cfg_t adc_config = {
        .max_store_buf_size = TUNER_ADC_BUFFER_POOL_SIZE,
        .conv_frame_size = TUNER_ADC_FRAME_SIZE,
    };
    ESP_ERROR_CHECK(adc_continuous_new_handle(&adc_config, &handle));

    adc_continuous_config_t dig_cfg = {
        .sample_freq_hz = TUNER_ADC_SAMPLE_RATE,
        .conv_mode = TUNER_ADC_CONV_MODE,
        .format = TUNER_ADC_OUTPUT_TYPE,
    };

    adc_digi_pattern_config_t adc_pattern[SOC_ADC_PATT_LEN_MAX] = {0};
    dig_cfg.pattern_num = channel_num;
    for (int i = 0; i < channel_num; i++) {
        adc_pattern[i].atten = TUNER_ADC_ATTEN;
        adc_pattern[i].channel = channel[i] & 0x7;
        adc_pattern[i].unit = TUNER_ADC_UNIT;
        adc_pattern[i].bit_width = TUNER_ADC_BIT_WIDTH;

        ESP_LOGI(TAG, "adc_pattern[%d].atten is :%"PRIx8, i, adc_pattern[i].atten);
        ESP_LOGI(TAG, "adc_pattern[%d].channel is :%"PRIx8, i, adc_pattern[i].channel);
        ESP_LOGI(TAG, "adc_pattern[%d].unit is :%"PRIx8, i, adc_pattern[i].unit);
    }
    dig_cfg.adc_pattern = adc_pattern;
    ESP_ERROR_CHECK(adc_continuous_config(handle, &dig_cfg));

    *out_handle = handle;
}

// static void oled_init(esp_lcd_panel_handle_t *out_handle) {
static void display_init(lv_disp_t **out_handle) {

    // Configure I2C as master

    i2c_master_bus_config_t i2c_mst_config = {
        .i2c_port = 0,
        .sda_io_num = SDA_OLED,
        .scl_io_num = SCL_OLED,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        // .intr_priority = 1,
        // .trans_queue_depth = 10,
        .flags = {
            .enable_internal_pullup = true // Internal pull-up resistor.
        }
    };

    i2c_master_bus_handle_t bus_handle;
    ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_mst_config, &bus_handle));

    // i2c_device_config_t dev_cfg = {
    //     .dev_addr_length = I2C_ADDR_BIT_LEN_7,
    //     .device_address = 0x3c,
    //     .scl_speed_hz = LCD_PIXEL_CLOCK_HZ,
    // };

    // i2c_master_dev_handle_t dev_handle;
    // ESP_ERROR_CHECK(i2c_master_bus_add_device(bus_handle, &dev_cfg, &dev_handle));

    // Configure the OLED

    esp_lcd_panel_io_handle_t io_handle = NULL;
    esp_lcd_panel_io_i2c_config_t io_config = {
        .dev_addr = 0x3c,
        .control_phase_bytes = 1, // refer to LCD spec
        .dc_bit_offset = 6,       // refer to LCD spec
        .lcd_cmd_bits = LCD_CMD_BITS,
        .lcd_param_bits = LCD_CMD_BITS,
        .scl_speed_hz = LCD_PIXEL_CLOCK_HZ,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_i2c_v2(bus_handle, &io_config, &io_handle));

    esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = RST_OLED,
        .bits_per_pixel = 1,
    };
    esp_lcd_panel_ssd1306_config_t ssd1306_config = {
        .height = 64
    };
    panel_config.vendor_config = &ssd1306_config;

    esp_lcd_panel_handle_t panel_handle = NULL;
    ESP_ERROR_CHECK(esp_lcd_new_panel_ssd1306(io_handle, &panel_config, &panel_handle));

    ESP_ERROR_CHECK(esp_lcd_panel_reset(panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_init(panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel_handle, true));

    ESP_LOGI(TAG, "Initialize LVGL");
    const lvgl_port_cfg_t lvgl_cfg = ESP_LVGL_PORT_INIT_CONFIG();
    lvgl_port_init(&lvgl_cfg);

    const lvgl_port_display_cfg_t disp_cfg = {
        .io_handle = io_handle,
        .panel_handle = panel_handle,
        .buffer_size = LCD_H_RES * LCD_V_RES,
        .double_buffer = true,
        .hres = LCD_H_RES,
        .vres = LCD_V_RES,
        .monochrome = true,
        .rotation = {
            .swap_xy = false,
            .mirror_x = false,
            .mirror_y = false,
        }
    };

    // *out_handle = panel_handle;
    lv_disp_t *disp = lvgl_port_add_disp(&disp_cfg);

    /* Rotation of the screen */
    lv_disp_set_rotation(disp, LV_DISP_ROT_NONE);

    create_frequency_label(disp);
    create_indicators(disp);
    
    *out_handle = disp;
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
    lv_disp_t *disp = NULL;
    display_init(&disp);

    while(1) {
        vTaskDelay(pdMS_TO_TICKS(30)); // ~33 times per second
        lv_task_handler();

        // Lock the mutex due to the LVGL APIs are not thread-safe
        if (lvgl_port_lock(0)) {
            // display_frequency(current_frequency);
            if (current_frequency > 0) {
                char noteName[8];
                int cents;
                get_pitch_name_and_cents_from_frequency(current_frequency, noteName, &cents);
                // ESP_LOGI("tuner", "%s - %d", noteName, cents);
                display_pitch(noteName, cents);
            } else {
                display_pitch("", 0);
            }
            // Release the mutex
            lvgl_port_unlock();
        }    
    }
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

    s_task_handle = xTaskGetCurrentTaskHandle();
    
    adc_continuous_handle_t handle = NULL;
    continuous_adc_init(channel, sizeof(channel) / sizeof(adc_channel_t), &handle);

    adc_continuous_evt_cbs_t cbs = {
        .on_conv_done = s_conv_done_cb,
    };
    ESP_ERROR_CHECK(adc_continuous_register_event_callbacks(handle, &cbs, NULL));
    ESP_ERROR_CHECK(adc_continuous_start(handle));

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
            // ret = adc_continuous_read(handle, result, EXAMPLE_READ_LEN, &ret_num, 0);
            // ret = adc_continuous_read(handle, adc_buffer, TUNER_ADC_BUFFER_POOL_SIZE * sizeof(adc_digi_output_data_t), &num_of_bytes_read, portMAX_DELAY);

            std::vector<float> in(TUNER_ADC_FRAME_SIZE);
            ret = adc_continuous_read(handle, adc_buffer, TUNER_ADC_FRAME_SIZE, &num_of_bytes_read, portMAX_DELAY);
            if (ret == ESP_OK) {
                // ESP_LOGI("TASK", "ret is %x, num_of_bytes_read is %"PRIu32" bytes", ret, num_of_bytes_read);

                // Get the data out of the ADC Conversion Result.
                int valuesStored = 0;
                float maxVal = 0;
                float minVal = MAXFLOAT;
                for (int i = 0; i < num_of_bytes_read; i += SOC_ADC_DIGI_RESULT_BYTES, valuesStored++) {
                    adc_digi_output_data_t *p = (adc_digi_output_data_t*)&adc_buffer[i];
                    // uint32_t chan_num = TUNER_ADC_GET_CHANNEL(p);

                    // First pass store the values in our float array
                    // to pass into qlib for processing.
                    in[valuesStored] = TUNER_ADC_GET_DATA(p);

                    // Track of the max and min values we see
                    // so we can use that to conver the array to
                    // values in between -1.0f and +1.0f.
                    if (in[valuesStored] > maxVal) {
                        maxVal = in[valuesStored];
                    }
                    if (in[valuesStored] < minVal) {
                        minVal = in[valuesStored];
                    }
                }

                // Bail out if the input does not meet the minimum criteria
                auto peakToPeakValue = maxVal - minVal;
                if (peakToPeakValue < TUNER_READING_DIFF_MINIMUM) {
                    current_frequency = -1; // Indicate to the UI that there's no frequency available
                    smoother.reset();
                    pd.reset();
                    continue;
                }

                // Normalize the values between -1.0 and +1.0 before
                // processing with qlib.
                float range = maxVal - minVal;
                float midVal = range / 2;
                // ESP_LOGI("min, max, range, mid:", "%f, %f, %f, %f", minVal, maxVal, range, midVal);
                for (auto i = 0; i < valuesStored; i++) {
                    // ESP_LOGI("adc", "%f", in[i]);
                    float newPosition = in[i] - midVal - minVal;
                    float normalizedValue = newPosition / midVal;
                    // ESP_LOGI("norm-val", "%f is now %f", in[i], normalizedValue);
                    in[i] = normalizedValue;
                }

                auto const&                 bits = pd.bits();
                auto const&                 edges = pd.edges();
                q::bitstream_acf<>          bacf{ bits };
                auto                        min_period = as_float(high_fs.period()) * TUNER_ADC_SAMPLE_RATE;
                
                q::peak_envelope_follower   env{ 30_ms, TUNER_ADC_SAMPLE_RATE };
                q::one_pole_lowpass         lp{high_fs, TUNER_ADC_SAMPLE_RATE};
                q::one_pole_lowpass         lp2(low_fs, TUNER_ADC_SAMPLE_RATE);

                constexpr float             slope = 1.0f/2;
                constexpr float             makeup_gain = 2;
                q::compressor               comp{ -18_dB, slope };
                q::clip                     clip;

                float                       onset_threshold = lin_float(-28_dB);
                float                       release_threshold = lin_float(-60_dB);
                float                       threshold = onset_threshold;

                // std::uint64_t               nanoseconds = 0;

                bool calculatedAFrequency = false;
                for (auto i = 0; i < valuesStored; ++i) {
                    float time = i / float(TUNER_ADC_SAMPLE_RATE);

                    auto s = in[i]; // input signal

                    // I've got the signal conditioning commented out right now
                    // because it actually is making the frequency readings
                    // NOT work. They probably just need to be tweaked a little.

                    // // Bandpass filter
                    s = lp(s);
                    s -= lp2(s);

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

                    // out[ch1] = s;

                    // if (time >= break_time) {
                    //     break_debug()
                    // }

                    // Pitch Detect
                    // auto start = std::chrono::high_resolution_clock::now();
                    bool ready = pd(s);
                    // auto elapsed = std::chrono::high_resolution_clock::now() - start;
                    // auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(elapsed);
                    // nanoseconds += duration.count();

                    calculatedAFrequency = ready;
                    // // Print the predicted frequency
                    // {
                    //     auto f = pd.predict_frequency() / as_double(high_fs);
                    //     out[ch5] = f;
                    //     ESP_LOGI("QLib", "Predicted Frequency: %f", f);
                    // }

                }

                // auto f = pd.get_frequency() / as_double(high_fs);
                if (calculatedAFrequency) {
                    auto f = pd.get_frequency();
                    current_frequency = smoother.smooth(f);
                    // current_frequency = f;
                    // ESP_LOGI("QLib", "Frequency: %f", f);
                    // ESP_LOGI("QLib", "%f - %f", f, current_frequency);
                    
                    // Store the frequency in the smoothing array

                }

                /**
                 * Because printing is slow, so every time you call `ulTaskNotifyTake`, it will immediately return.
                 * To avoid a task watchdog timeout, add a delay here. When you replace the way you process the data,
                 * usually you don't need this delay (as this task will block for a while).
                 */
                vTaskDelay(1);
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