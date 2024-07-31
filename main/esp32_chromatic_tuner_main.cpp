/*
 * SPDX-FileCopyrightText: 2021-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <cmath>
#include <string.h>
#include <stdio.h>
#include "sdkconfig.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_adc/adc_continuous.h"
#include <q/pitch/pitch_detector.hpp>
#include <q/fx/dynamic.hpp>
#include <q/fx/clip.hpp>
#include <q/support/decibel.hpp>
#include <q/support/duration.hpp>
#include <q/support/literals.hpp>
#include <q/support/pitch_names.hpp>

#define EXAMPLE_ADC_UNIT                    ADC_UNIT_1
#define _EXAMPLE_ADC_UNIT_STR(unit)         #unit
#define EXAMPLE_ADC_UNIT_STR(unit)          _EXAMPLE_ADC_UNIT_STR(unit)
#define EXAMPLE_ADC_CONV_MODE               ADC_CONV_SINGLE_UNIT_1
// #define EXAMPLE_ADC_CONV_MODE               ADC_CONV_BOTH_UNIT
#define EXAMPLE_ADC_ATTEN                   ADC_ATTEN_DB_12
#define EXAMPLE_ADC_BIT_WIDTH               SOC_ADC_DIGI_MAX_BITWIDTH

#if CONFIG_IDF_TARGET_ESP32 || CONFIG_IDF_TARGET_ESP32S2
#define EXAMPLE_ADC_OUTPUT_TYPE             ADC_DIGI_OUTPUT_FORMAT_TYPE1
#define EXAMPLE_ADC_GET_CHANNEL(p_data)     ((p_data)->type1.channel)
#define EXAMPLE_ADC_GET_DATA(p_data)        ((p_data)->type1.data)
#else
// #define EXAMPLE_ADC_OUTPUT_TYPE             ADC_DIGI_OUTPUT_FORMAT_TYPE2
// #define EXAMPLE_ADC_GET_CHANNEL(p_data)     ((p_data)->type2.channel)
// #define EXAMPLE_ADC_GET_DATA(p_data)        ((p_data)->type2.data)
#define EXAMPLE_ADC_OUTPUT_TYPE             ADC_DIGI_OUTPUT_FORMAT_TYPE2
#define EXAMPLE_ADC_GET_CHANNEL(p_data)     ((p_data)->type2.channel)
#define EXAMPLE_ADC_GET_DATA(p_data)        ((p_data)->type2.data)
#endif

// #define BUF_SIZE    1024 * SOC_ADC_DIGI_DATA_BYTES_PER_CONV
// #define FRAME_SIZE  256 * SOC_ADC_DIGI_DATA_BYTES_PER_CONV
#define BUF_SIZE    2048 * SOC_ADC_DIGI_DATA_BYTES_PER_CONV
#define FRAME_SIZE  512 * SOC_ADC_DIGI_DATA_BYTES_PER_CONV

// The minimum threshold sample frequency is 20 * 1000 according
// to esp-idf/components/soc/esp32/include/soc/soc_caps.h
// #define SOC_ADC_SAMPLE_FREQ_THRES_LOW (20*1000)
// #define SAMPLE_RATE (20 * 1000)
// #define SAMPLE_RATE SOC_ADC_SAMPLE_FREQ_THRES_LOW
#define SAMPLE_RATE 6000

using namespace cycfi::q::pitch_names;
using frequency = cycfi::q::frequency;
CONSTEXPR frequency low_fs  = Fs[1];

// #define EXAMPLE_READ_LEN                    256

#if CONFIG_IDF_TARGET_ESP32
// static adc_channel_t channel[2] = {ADC_CHANNEL_6, ADC_CHANNEL_7};
static adc_channel_t channel[1] = {ADC_CHANNEL_4};
#else
// static adc_channel_t channel[2] = {ADC_CHANNEL_2, ADC_CHANNEL_3};
static adc_channel_t channel[1] = {ADC_CHANNEL_4};
#endif

namespace q = cycfi::q;
using namespace q::literals;
using std::fixed;

// constexpr auto sps = 44100;

static TaskHandle_t s_task_handle;
static const char *TAG = "TUNER";

static bool IRAM_ATTR s_conv_done_cb(adc_continuous_handle_t handle, const adc_continuous_evt_data_t *edata, void *user_data)
{
    BaseType_t mustYield = pdFALSE;
    //Notify that ADC continuous driver has done enough number of conversions
    vTaskNotifyGiveFromISR(s_task_handle, &mustYield);

    return (mustYield == pdTRUE);
}

static void continuous_adc_init(adc_channel_t *channel, uint8_t channel_num, adc_continuous_handle_t *out_handle)
{
    adc_continuous_handle_t handle = NULL;

    // adc_continuous_handle_cfg_t adc_config = {
    //     .max_store_buf_size = 1024,
    //     .conv_frame_size = EXAMPLE_READ_LEN,
    // };
    adc_continuous_handle_cfg_t adc_config = {
        // .max_store_buf_size = BUF_SIZE * sizeof(adc_digi_output_data_t),
        // .conv_frame_size = BUF_SIZE,
        .max_store_buf_size = BUF_SIZE,
        .conv_frame_size = FRAME_SIZE,
    };
    ESP_ERROR_CHECK(adc_continuous_new_handle(&adc_config, &handle));

    adc_continuous_config_t dig_cfg = {
        .sample_freq_hz = SAMPLE_RATE,
        .conv_mode = EXAMPLE_ADC_CONV_MODE,
        .format = EXAMPLE_ADC_OUTPUT_TYPE,
    };

    adc_digi_pattern_config_t adc_pattern[SOC_ADC_PATT_LEN_MAX] = {0};
    dig_cfg.pattern_num = channel_num;
    for (int i = 0; i < channel_num; i++) {
        adc_pattern[i].atten = EXAMPLE_ADC_ATTEN;
        adc_pattern[i].channel = channel[i] & 0x7;
        adc_pattern[i].unit = EXAMPLE_ADC_UNIT;
        adc_pattern[i].bit_width = EXAMPLE_ADC_BIT_WIDTH;

        ESP_LOGI(TAG, "adc_pattern[%d].atten is :%"PRIx8, i, adc_pattern[i].atten);
        ESP_LOGI(TAG, "adc_pattern[%d].channel is :%"PRIx8, i, adc_pattern[i].channel);
        ESP_LOGI(TAG, "adc_pattern[%d].unit is :%"PRIx8, i, adc_pattern[i].unit);
    }
    dig_cfg.adc_pattern = adc_pattern;
    ESP_ERROR_CHECK(adc_continuous_config(handle, &dig_cfg));

    *out_handle = handle;
}

extern "C" void app_main() {
    esp_err_t ret;
    // uint32_t ret_num = 0;
    uint32_t bytes_read = 0;
    // uint8_t *buffer = (uint8_t *)malloc(BUF_SIZE * sizeof(adc_digi_output_data_t));
    uint8_t *buffer = (uint8_t *)malloc(FRAME_SIZE);
    if (buffer == NULL) {
        ESP_LOGI(TAG, "Failed to allocate memory for buffer");
        return;
    }
    memset(buffer, 0xcc, FRAME_SIZE);

    // uint8_t result[EXAMPLE_READ_LEN] = {0};
    // memset(result, 0xcc, EXAMPLE_READ_LEN);

    int64_t samples = 0;
    int64_t current_us;
    int64_t previous_us;

    s_task_handle = xTaskGetCurrentTaskHandle();

    adc_continuous_handle_t handle = NULL;
    continuous_adc_init(channel, sizeof(channel) / sizeof(adc_channel_t), &handle);

    adc_continuous_evt_cbs_t cbs = {
        .on_conv_done = s_conv_done_cb,
    };
    ESP_ERROR_CHECK(adc_continuous_register_event_callbacks(handle, &cbs, NULL));
    ESP_ERROR_CHECK(adc_continuous_start(handle));

    previous_us = esp_timer_get_time();
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

        char unit[] = EXAMPLE_ADC_UNIT_STR(EXAMPLE_ADC_UNIT);

        while (1) {
            // ret = adc_continuous_read(handle, result, EXAMPLE_READ_LEN, &ret_num, 0);
            // ret = adc_continuous_read(handle, buffer, BUF_SIZE * sizeof(adc_digi_output_data_t), &bytes_read, portMAX_DELAY);

            std::vector<float> in(FRAME_SIZE);
            ret = adc_continuous_read(handle, buffer, FRAME_SIZE, &bytes_read, portMAX_DELAY);
            if (ret == ESP_OK) {
                // Get the data out of the ADC Conversion Result
                for (int i = 0; i < bytes_read; i += SOC_ADC_DIGI_RESULT_BYTES) {
                    adc_digi_output_data_t *p = (adc_digi_output_data_t*)&buffer[i];
                    // uint32_t chan_num = EXAMPLE_ADC_GET_CHANNEL(p);
                    in[i] = EXAMPLE_ADC_GET_DATA(p);
                    // uint32_t data = EXAMPLE_ADC_GET_DATA(p);
                    ESP_LOGI("freq", "%f", in[i]);
                }

                // ESP_LOGI("TASK", "ret is %x, bytes_read is %"PRIu32" bytes", ret, bytes_read);
                // samples += (bytes_read/SOC_ADC_DIGI_RESULT_BYTES);
                // if (samples >= SAMPLE_RATE) {
                //     current_us = esp_timer_get_time();
                //     int64_t sps = samples * 1000000 / (current_us - previous_us);
                //     previous_us = esp_timer_get_time();
// 
                    // printf("samples = %lld, time = %lld us, sps = %lld\n", samples, ((current_us)), sps);
                    // q::pitch_detector pd(30_Hz, 2700_Hz, sps, -45_dB);

                    ////////////////////////////////////////////////////////////////////////////
                    // Output
                    auto data_length = bytes_read / SOC_ADC_DIGI_RESULT_BYTES;
                    constexpr auto n_channels = 5;
                    std::vector<float> out(data_length * n_channels);
                    std::fill(out.begin(), out.end(), 0); // Fill the output vector with zeros

                    // std::vector<float> out(src.length() * n_channels);
                    // std::fill(out.begin(), out.end(), 0);

                    frequency high_fs   = low_fs * 5;
                    q::pitch_detector   pd(low_fs, high_fs, SAMPLE_RATE, -40_dB);
                    auto const&         bits = pd.bits();
                    auto const&         edges = pd.edges();
                    q::bitstream_acf<>  bacf{ bits };
                    auto                min_period = as_float(high_fs.period()) * SAMPLE_RATE;
                    
                    q::peak_envelope_follower   env{ 30_ms, SAMPLE_RATE };
                    q::one_pole_lowpass         lp{ high_fs, SAMPLE_RATE };
                    q::one_pole_lowpass         lp2{ low_fs, SAMPLE_RATE };

                    constexpr float             slope = 1.0f/4;
                    constexpr float             makeup_gain = 4;
                    q::compressor               comp{ -18_dB, slope };
                    q::clip                     clip;

                    float                       onset_threshold = lin_float(-28_dB);
                    float                       release_threshold = lin_float(-60_dB);
                    float                       threshold = onset_threshold;

                    std::uint64_t               nanoseconds = 0;

                    // for (int i = 0; i < ret_num; i += SOC_ADC_DIGI_RESULT_BYTES) {
                    // for (int i = 0; i < bytes_read; i += SOC_ADC_DIGI_RESULT_BYTES) {
                        // adc_digi_output_data_t *p = (adc_digi_output_data_t*)&buffer[i];
                        // uint32_t chan_num = EXAMPLE_ADC_GET_CHANNEL(p);
                        // uint32_t data = EXAMPLE_ADC_GET_DATA(p);
                    for (auto i = 0; i != data_length; i++) {
                        auto pos = (i / SOC_ADC_DIGI_RESULT_BYTES) * n_channels;
                        auto ch1 = pos;         // input
                        auto ch2 = pos + 1;     // zero crossings
                        auto ch3 = pos + 2;     // bacf
                        auto ch4 = pos + 3;     // frequency
                        auto ch5 = pos + 4;     // predicted frequency

                        float time = i / SOC_ADC_DIGI_RESULT_BYTES / SAMPLE_RATE;

                        // auto s = data; // signal
                        auto s = in[i]; // input signal

                        // Bandpass filter
                        s = lp(s);
                        s -= lp2(s);

                        // Envelope
                        auto e = env(std::abs(static_cast<int>(s)));
                        auto e_db = q::lin_to_db(e);

                        if (e > threshold) {
                            // Compressor + make-up gain + hard clip
                            auto gain = cycfi::q::lin_float(comp(e_db)) * makeup_gain;
                            s = clip(s * gain);
                            threshold = release_threshold;
                        } else {
                            s = 0.0f;
                            threshold = onset_threshold;
                        }

                        out[ch1] = s;

                        // if (time >= break_time) {
                        //     break_debug()
                        // }

                        // Pitch Detect
                        auto start = std::chrono::high_resolution_clock::now();
                        bool ready = pd(s);
                        auto elapsed = std::chrono::high_resolution_clock::now() - start;
                        auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(elapsed);
                        nanoseconds += duration.count();

                        out[ch2] = -0.8; // placeholder for bitset bits
                        out[ch3] = 0.0f; // placeholder for autocorrelation results
                        out[ch4] = -0.8; // placeholder for frequency

                        if (ready) {
                            auto frame = edges.frame() + (edges.window_size() / 2);
                            auto extra = frame - edges.window_size();
                            auto size = bits.size();
                            // Write the frequency info to CSV
                            // {
                            //     auto f = pd.get_frequency();
                            //     auto p = pd.periodicity();
                            //     auto f2 = float(SAMPLE_RATE) / pd.get_period_detector().fundamental()._period;
                            //     auto fr = pd.frames_after_shift();
                            // }

                            // Print the frequency
                            {
                                auto f = pd.get_frequency() / as_double(high_fs);
                                auto out_i = (&out[ch4] - (((size - 1) + extra) * n_channels));
                                for (auto i = 0; i != size; ++i) {
                                    *out_i = f;
                                    out_i += n_channels;
                                }
                            }
                        }

                        // Print the predicted frequency
                        {
                            auto f = pd.predict_frequency() / as_double(high_fs);
                            out[ch5] = f;
                            ESP_LOGI("QLib", "Predicted Frequency: %f", f);
                        }
                    }



                    //     bool is_ready = pd((float)data);
                    //     if (is_ready) {
                    //         auto frequency = pd.get_frequency();
                    //         if (frequency != 0.0f) {
                    //             ESP_LOGI(TAG, "Frequency: %f", frequency);                            
                    //         }
                    //     }
                    // }
//                     samples = 0;
//                 }
// ESP_LOGI("TASK", "samples is %lld", samples);

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

    free(buffer);

    ESP_ERROR_CHECK(adc_continuous_stop(handle));
    ESP_ERROR_CHECK(adc_continuous_deinit(handle));
}
