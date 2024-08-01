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

// #define TUNER_ADC_BUFFER_POOL_SIZE    2048 * SOC_ADC_DIGI_DATA_BYTES_PER_CONV
// #define TUNER_ADC_FRAME_SIZE  512 * SOC_ADC_DIGI_DATA_BYTES_PER_CONV
#define TUNER_ADC_BUFFER_POOL_SIZE      2048
#define TUNER_ADC_FRAME_SIZE            512

// The minimum threshold sample frequency is 20 * 1000 according
// to esp-idf/components/soc/esp32/include/soc/soc_caps.h.
// As far as I understand, this is the frequency sampling
// rate (Samples Per Second/sps).
// #define TUNER_ADC_SAMPLE_RATE 20 * 1000
#define TUNER_ADC_SAMPLE_RATE 10000

using namespace cycfi::q::pitch_names;
using frequency = cycfi::q::frequency;
CONSTEXPR frequency low_fs  = Fs[1];

#if CONFIG_IDF_TARGET_ESP32
// static adc_channel_t channel[2] = {ADC_CHANNEL_6, ADC_CHANNEL_7};
static adc_channel_t channel[1] = {ADC_CHANNEL_4};
#else
// static adc_channel_t channel[2] = {ADC_CHANNEL_2, ADC_CHANNEL_3};
static adc_channel_t channel[1] = {ADC_CHANNEL_1};
#endif

namespace q = cycfi::q;
using namespace q::literals;
using std::fixed;

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

extern "C" void app_main() {
    esp_err_t ret;
    uint32_t num_of_bytes_read = 0;
    uint8_t *adc_buffer = (uint8_t *)malloc(TUNER_ADC_FRAME_SIZE);
    if (adc_buffer == NULL) {
        ESP_LOGI(TAG, "Failed to allocate memory for buffer");
        return;
    }
    memset(adc_buffer, 0xcc, TUNER_ADC_FRAME_SIZE);

    // Get the pitch detector ready
    frequency high_fs   = low_fs * 300;
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

                // Normalize the values between -1.0 and +1.0 before
                // processing with qlib.
                float range = maxVal - minVal;
                float midVal = range / 2;
                // ESP_LOGI("min, max, range, mid:", "%f, %f, %f, %f", minVal, maxVal, range, midVal);
                for (auto i = 0; i < valuesStored; i++) {
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
                q::one_pole_lowpass         lp{ high_fs, TUNER_ADC_SAMPLE_RATE };
                q::one_pole_lowpass         lp2{ low_fs, TUNER_ADC_SAMPLE_RATE };

                constexpr float             slope = 1.0f/4;
                constexpr float             makeup_gain = 4;
                q::compressor               comp{ -18_dB, slope };
                q::clip                     clip;

                float                       onset_threshold = lin_float(-28_dB);
                float                       release_threshold = lin_float(-60_dB);
                float                       threshold = onset_threshold;

                std::uint64_t               nanoseconds = 0;

                bool calculatedAFrequency = false;
                for (auto i = 0; i < valuesStored; ++i) {
                    float time = i / float(TUNER_ADC_SAMPLE_RATE);

                    auto s = in[i]; // input signal

                    // I've got the signal conditioning commented out right now
                    // because it actually is making the frequency readings
                    // NOT work. They probably just need to be tweaked a little.

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

                    // out[ch1] = s;

                    // if (time >= break_time) {
                    //     break_debug()
                    // }

                    // Pitch Detect
                    auto start = std::chrono::high_resolution_clock::now();
                    bool ready = pd(s);
                    auto elapsed = std::chrono::high_resolution_clock::now() - start;
                    auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(elapsed);
                    nanoseconds += duration.count();

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
                    ESP_LOGI("QLib", "Frequency: %f", f);
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
