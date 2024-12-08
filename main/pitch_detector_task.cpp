/*=============================================================================
    Copyright (c) 2024 Boyd Timothy. All rights reserved.

    Distributed under the MIT License [ https://opensource.org/licenses/MIT ]
=============================================================================*/
#include "pitch_detector_task.h"

#include "defines.h"
#include "globals.h"
#include "UserSettings.h"

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "esp_adc/adc_continuous.h"
#include "esp_timer.h"

//
// Q DSP Library for Pitch Detection
//
#include <q/pitch/pitch_detector.hpp>
#include <q/fx/dynamic.hpp>
#include <q/fx/clip.hpp>
// #include <q/fx/signal_conditioner.hpp>
#include <q/support/decibel.hpp>
#include <q/support/duration.hpp>
#include <q/support/literals.hpp>
#include <q/support/pitch_names.hpp>
#include <q/support/pitch.hpp>

//
// Smoothing Filters
//
#include "exponential_smoother.hpp"
#include "OneEuroFilter.h"
// #include "MovingAverage.hpp"

static const char *TAG = "PitchDetector";

namespace q = cycfi::q;
using namespace q::literals;
using std::fixed;

using namespace cycfi::q::pitch_names;
using frequency = cycfi::q::frequency;
using pitch = cycfi::q::pitch;
CONSTEXPR frequency low_fs = cycfi::q::pitch_names::C[1];
CONSTEXPR frequency high_fs = cycfi::q::pitch_names::C[7]; // Setting this higher helps to catch the high harmonics

static adc_channel_t channel[1] = {ADC_CHANNEL_7}; // ESP32-WROOM-32 CYD - GPIO 35 (ADC1_CH7)

// 1EU Filter Initialization Params
static const double euFilterFreq = EU_FILTER_ESTIMATED_FREQ; // I believe this means no guess as to what the incoming frequency will initially be
static const double mincutoff = EU_FILTER_MIN_CUTOFF;
static const double dcutoff = EU_FILTER_DERIVATIVE_CUTOFF;

ExponentialSmoother smoother(DEFAULT_EXP_SMOOTHING);
OneEuroFilter oneEUFilter(euFilterFreq, mincutoff, DEFAULT_ONE_EU_BETA, dcutoff);
// MovingAverage movingAverage(DEFAULT_MOVING_AVG_WINDOW);

extern UserSettings *userSettings;

static TaskHandle_t s_task_handle;
static bool IRAM_ATTR s_conv_done_cb(adc_continuous_handle_t handle, const adc_continuous_evt_data_t *edata, void *user_data)
{
    BaseType_t mustYield = pdFALSE;
    //Notify that ADC continuous driver has done enough number of conversions
    vTaskNotifyGiveFromISR(s_task_handle, &mustYield);

    return (mustYield == pdTRUE);
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

void pitch_detector_task(void *pvParameter) {
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

    // auto sc_conf = q::signal_conditioner::config{};
    // auto sig_cond = q::signal_conditioner{sc_conf, low_fs, high_fs, TUNER_ADC_SAMPLE_RATE};

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
            if (userSettings == NULL || userSettings->isShowingSettings()) {
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
                    set_current_frequency(-1); // Indicate to the UI that there's no frequency available
                    oneEUFilter.reset(); // Reset the 1EU filter so the next frequency it detects will be as fast as possible
                    smoother.reset();
                    // movingAverage.reset();
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

                        bool use1EUFilterFirst = true; // TODO: This may never be needed. Need to test which "feels" better for tuning
                        if (use1EUFilterFirst) {
                            // 1EU Filtering
                            f = (float)oneEUFilter.filter((double)f, (TimeStamp)time_seconds);

                            // Simple Exponential Smoothing
                            f = smoother.smooth(f);
                        } else {
                            // Simple Expoential Smoothing
                            f = smoother.smooth(f);

                            // 1EU Filtering
                            f = (float)oneEUFilter.filter((double)f, (TimeStamp)time_seconds);
                        }

                        // Moving average (makes it BAD!)
                        // f = movingAverage.addValue(f);
                        f = f / WEIRD_ESP32_WROOM_32_FREQ_FIX_FACTOR; // Use the weird factor only on ESP32-WROOM-32 (which the CYD is)
                        set_current_frequency(f);
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

void update_pitch_detector_user_settings() {
    oneEUFilter.setBeta(userSettings->oneEUBeta);
    smoother.setAmount(userSettings->expSmoothing);
    // movingAverage.setWindowSize(userSettings->movingAvgWindow);
}
