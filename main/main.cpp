#include "main.h"
#include <esp_adc/adc_continuous.h>
#include <driver/twai.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>
#include <driver/gpio.h>

constexpr const char* logTag = "Main";

namespace Pins {
    // CAN
    constexpr gpio_num_t canTx = GPIO_NUM_21;
    constexpr gpio_num_t canRx = GPIO_NUM_22;
};

namespace SamplingParameters {
    constexpr int displayRefreshRate = 60;
    constexpr int adcChannels = 4; // RGB + audio
    constexpr int minimumSampleFrequency = SOC_ADC_SAMPLE_FREQ_THRES_LOW;
    // The ESP32 has an error that causes the actual sample frequency to be lower than expected
    // Compensate for this
    constexpr double sampleFrequencyCorrectionFactor = 1.2226;
    // We want to take an equal number of samples per display frame for each channel,
    // so we need a sample frequency that produces a whole number of samples per display frame
    // If the minimum sample frequency doesn't produce this then we take one extra sample per
    // display frame and calculate the corresponding sample frequency
    // Note that the described error also causes a slightly different number of samples per
    // channel, so this is a best effort attempt
    constexpr int singleDisplayChannelSamplesPerScreenFrame = minimumSampleFrequency % displayRefreshRate == 0 ? minimumSampleFrequency / displayRefreshRate : (minimumSampleFrequency / displayRefreshRate) + 1;
    constexpr int sampleFrequency = singleDisplayChannelSamplesPerScreenFrame * adcChannels * displayRefreshRate * sampleFrequencyCorrectionFactor;
    // Per read operation we get samples for the display channels plus the audio channel
    constexpr int samplesPerRead = singleDisplayChannelSamplesPerScreenFrame * adcChannels;
}

Processing processing(Pins::canTx, Pins::canRx);

void CppMain()
{
    adc_continuous_handle_t adcContinuousHandle = NULL;
    adc_continuous_handle_cfg_t adcContinuousHandleCfg = {
        .max_store_buf_size = 2 * SamplingParameters::samplesPerRead * SOC_ADC_DIGI_RESULT_BYTES,
        .conv_frame_size = SamplingParameters::samplesPerRead * SOC_ADC_DIGI_RESULT_BYTES
    };
    ESP_ERROR_CHECK(adc_continuous_new_handle(&adcContinuousHandleCfg, &adcContinuousHandle));

    adc_digi_pattern_config_t adcDigiPatternConfig[SamplingParameters::adcChannels] = {};
    adcDigiPatternConfig[0].atten = ADC_ATTEN_DB_0;
    adcDigiPatternConfig[0].channel = ADC_CHANNEL_0;
    adcDigiPatternConfig[0].unit = ADC_UNIT_1;
    adcDigiPatternConfig[0].bit_width = SOC_ADC_DIGI_MAX_BITWIDTH;
    adcDigiPatternConfig[1].atten = ADC_ATTEN_DB_0;
    adcDigiPatternConfig[1].channel = ADC_CHANNEL_3;
    adcDigiPatternConfig[1].unit = ADC_UNIT_1;
    adcDigiPatternConfig[1].bit_width = SOC_ADC_DIGI_MAX_BITWIDTH;
    adcDigiPatternConfig[2].atten = ADC_ATTEN_DB_0;
    adcDigiPatternConfig[2].channel = ADC_CHANNEL_6;
    adcDigiPatternConfig[2].unit = ADC_UNIT_1;
    adcDigiPatternConfig[2].bit_width = SOC_ADC_DIGI_MAX_BITWIDTH;
    adcDigiPatternConfig[3].atten = ADC_ATTEN_DB_0;
    adcDigiPatternConfig[3].channel = ADC_CHANNEL_7;
    adcDigiPatternConfig[3].unit = ADC_UNIT_1;
    adcDigiPatternConfig[3].bit_width = SOC_ADC_DIGI_MAX_BITWIDTH;
    adc_continuous_config_t adcContinuousConfig = {
        .pattern_num = SamplingParameters::adcChannels,
        .adc_pattern = adcDigiPatternConfig,
        .sample_freq_hz = SamplingParameters::sampleFrequency,
        .conv_mode = ADC_CONV_SINGLE_UNIT_1,
        .format = ADC_DIGI_OUTPUT_FORMAT_TYPE1
    };
    ESP_ERROR_CHECK(adc_continuous_config(adcContinuousHandle, &adcContinuousConfig));
    ESP_ERROR_CHECK(adc_continuous_start(adcContinuousHandle));

    processing.Start();

    gpio_config_t io_conf = {};
    io_conf.pin_bit_mask = 1ULL << GPIO_NUM_2;
    io_conf.mode = GPIO_MODE_OUTPUT;
    gpio_config(&io_conf);
    io_conf.pin_bit_mask = 1ULL << GPIO_NUM_25;
    io_conf.mode = GPIO_MODE_INPUT;
    gpio_config(&io_conf);
    io_conf.pin_bit_mask = 1ULL << GPIO_NUM_26;
    io_conf.mode = GPIO_MODE_INPUT;
    gpio_config(&io_conf);
    io_conf.pin_bit_mask = 1ULL << GPIO_NUM_27;
    io_conf.mode = GPIO_MODE_INPUT;
    gpio_config(&io_conf);
    constexpr int slidingWindowSize = 1;
    constexpr int slidingWindowGranularity = 30;
    int redSlidingWindow[slidingWindowSize] = {};
    int greenSlidingWindow[slidingWindowSize] = {};
    int blueSlidingWindow[slidingWindowSize] = {};
    int slidingWindowWriteIndex = 0;
    int audio = 0;
    int screenSwitchAtMcu = 0;
    int screenSwitchAtRelay = 0;
    int cameraEnable = 0;
    int count = 0;
    while (true)
    {
        gpio_set_level(GPIO_NUM_2, (count + 1) % 2);

        redSlidingWindow[slidingWindowWriteIndex] = 0;
        greenSlidingWindow[slidingWindowWriteIndex] = 0;
        blueSlidingWindow[slidingWindowWriteIndex] = 0;
        uint32_t readLength = 0;
        adc_digi_output_data_t result[SamplingParameters::samplesPerRead] = {};
        // Because of an error in the ESP32 there's a slightly different number of samples per channel
        // It is necessary to keep count of the number of samples for each channel
        int redSampleCount = 0;
        int greenSampleCount = 0;
        int blueSampleCount = 0;
        int audioSampleCount = 0;
        for (int measurementCount = 0; measurementCount < slidingWindowGranularity; measurementCount++)
        {
            if (adc_continuous_read(adcContinuousHandle, (uint8_t*)result, sizeof(result), &readLength, portMAX_DELAY) == ESP_OK) {
                for (int resultIndex = 0; resultIndex < SamplingParameters::samplesPerRead; resultIndex++) {
                    switch (result[resultIndex].type1.channel) {
                    case ADC_CHANNEL_0:
                        redSlidingWindow[slidingWindowWriteIndex] += result[resultIndex].type1.data;
                        redSampleCount++;
                        break;
                    case ADC_CHANNEL_3:
                        greenSlidingWindow[slidingWindowWriteIndex] += result[resultIndex].type1.data;
                        greenSampleCount++;
                        break;
                    case ADC_CHANNEL_6:
                        blueSlidingWindow[slidingWindowWriteIndex] += result[resultIndex].type1.data;
                        blueSampleCount++;
                        break;
                    case ADC_CHANNEL_7:
                        audio += result[resultIndex].type1.data;
                        audioSampleCount++;
                        break;
                    }
                }
            }
        }
        slidingWindowWriteIndex++;
        slidingWindowWriteIndex %= slidingWindowSize;

        screenSwitchAtMcu = gpio_get_level(GPIO_NUM_25);
        screenSwitchAtRelay = gpio_get_level(GPIO_NUM_26);
        cameraEnable = gpio_get_level(GPIO_NUM_27);

        int redMovingAverage = 0;
        int greenMovingAverage = 0;
        int blueMovingAverage = 0;
        for (int movingWindowReadIndex = 0; movingWindowReadIndex < slidingWindowSize; movingWindowReadIndex++)
        {
            redMovingAverage += redSlidingWindow[movingWindowReadIndex];
            greenMovingAverage += greenSlidingWindow[movingWindowReadIndex];
            blueMovingAverage += blueSlidingWindow[movingWindowReadIndex];
        }
        redMovingAverage /= redSampleCount;
        greenMovingAverage /= greenSampleCount;
        blueMovingAverage /= blueSampleCount;
        audio /= audioSampleCount;
        ESP_LOGI(logTag, "   %04d    %04d    %04d    %04d", redSampleCount, greenSampleCount, blueSampleCount, audioSampleCount);
        ESP_LOGI(logTag, "   %04d    %04d    %04d    %04d    %d    %d    %d", redMovingAverage, greenMovingAverage, blueMovingAverage, audio, screenSwitchAtMcu, screenSwitchAtRelay, cameraEnable);

        count++;
    }
}

extern "C"
{
    void app_main()
    {
        CppMain();
    }
}

Processing::Processing(gpio_num_t canTxPin, gpio_num_t canRxPin) :
    _canTxPin(canTxPin),
    _canRxPin(canRxPin),
    _processingMutex(xSemaphoreCreateMutex())
{
}

void Processing::Start()
{
    twai_general_config_t g_config = TWAI_GENERAL_CONFIG_DEFAULT(_canTxPin, _canRxPin, TWAI_MODE_NORMAL);
    g_config.intr_flags |= ESP_INTR_FLAG_IRAM;
    twai_timing_config_t t_config = TWAI_TIMING_CONFIG_100KBITS();
    twai_filter_config_t f_config = TWAI_FILTER_CONFIG_ACCEPT_ALL();

    ESP_ERROR_CHECK(twai_driver_install(&g_config, &t_config, &f_config));
    ESP_LOGI(_logTag, "Installed TWAI driver");

    ESP_ERROR_CHECK(twai_start());
    ESP_LOGI(_logTag, "Started TWAI driver");

    xTaskCreate(
        CanReceiveTask,
        "CanReceiveTask",
        4096,
        this,
        configMAX_PRIORITIES / 2,
        nullptr
    );
}

void Processing::CanReceiveTask(void* pvParameters)
{
    Processing* processing = (Processing*)pvParameters;
    twai_message_t message;
    while (true)
    {
        if (ESP_ERROR_CHECK_WITHOUT_ABORT(twai_receive(&message, portMAX_DELAY)) == ESP_OK)
        {
            if (!(message.rtr))
            {
                xSemaphoreTake(processing->_processingMutex, portMAX_DELAY);
                // for (auto const& iOutput : canInterface->_iOutputs)
                // {
                //     iOutput->IPsCanInterfaceHandleCanMessage(message.identifier, message.data_length_code, message.data);
                // }
                xSemaphoreGive(processing->_processingMutex);
            }
        }
    }
}