#include "AndroidAutoUnitAssist.h"
#include <Arduino.h>
#include <driver/adc.h>
#include <driver/twai.h>
#ifdef USE_ESP32_FRAMEWORK_ARDUINO
#include "esphome/core/log.h"
using namespace esphome;
#else
#include <esp_log.h>
#endif
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>
#include <driver/gpio.h>

void AndroidAutoUnitAssist::Setup() {
    _processingMutex = xSemaphoreCreateMutex();

    adc1_config_width(ADC_WIDTH_BIT_12);
    adc1_config_channel_atten(ADC1_CHANNEL_0, ADC_ATTEN_DB_0);
    adc1_config_channel_atten(ADC1_CHANNEL_3, ADC_ATTEN_DB_0);
    adc1_config_channel_atten(ADC1_CHANNEL_6, ADC_ATTEN_DB_0);
    adc1_config_channel_atten(ADC1_CHANNEL_7, ADC_ATTEN_DB_0);

    xTaskCreate(
        ReadInputsTask,
        "ReadInputsTask",
        4096,
        this,
        configMAX_PRIORITIES / 2,
        nullptr
    );

    twai_general_config_t g_config = TWAI_GENERAL_CONFIG_DEFAULT(_pins.canTx, _pins.canRx, TWAI_MODE_NORMAL);
    // g_config.intr_flags |= ESP_INTR_FLAG_IRAM;
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
}

void AndroidAutoUnitAssist::Loop() {
    
}

void AndroidAutoUnitAssist::ProcessEvent(EventType eventType) {
    CpAAScreenActiveDetectionStateMachine(eventType);
}

void AndroidAutoUnitAssist::CpAAScreenActiveDetectionStateMachine(EventType eventType) {
    auto StateTransition = [&](CpAAScreenActiveDetectionState state) {
        _cpAAScreenActiveDetectionState = state;
        CpAAScreenActiveDetectionStateMachine(EventType::stateEntry);
        };

    switch (_cpAAScreenActiveDetectionState) {
    case CpAAScreenActiveDetectionState::start:
        switch (eventType) {
        case EventType::stateEntry:
            // if (_timer2Period > 0 && _menuScreenLevel > 0) {
            //     StartTimer(2);
            // }
            break;
        case EventType::timer2Expiry:
        case EventType::newMenuScreenLevelDetected:
            StateTransition(CpAAScreenActiveDetectionState::detectionActive);
            break;
        default:
            break;
        }
        break;
    case CpAAScreenActiveDetectionState::detectionActive:
        switch (eventType) {
        case EventType::stateEntry:
        case EventType::cameraEnableChanged:
        case EventType::menuScreenActiveChanged:
            // if (!CameraEnabled() && !MenuLevelActive()) {

            // }
            break;
        default:
            break;
        }
        break;
    }
}

void AndroidAutoUnitAssist::StartTimer(int timerNumber) {
    switch (timerNumber) {
    case 2:
        _timer2StartTime = xTaskGetTickCount();
        break;
    }
}

void AndroidAutoUnitAssist::ReadInputsTask(void* pvParameters) {
    constexpr int slidingWindowSize = 1;
    constexpr int slidingWindowGranularity = 30;
    int redSlidingWindow[slidingWindowSize] = {};
    int greenSlidingWindow[slidingWindowSize] = {};
    int blueSlidingWindow[slidingWindowSize] = {};
    int slidingWindowWriteIndex = 0;
    int screenSwitchAtMcu = 0;
    int screenSwitchAtRelay = 0;
    int cameraEnable = 0;
    int count = 0;
    while (true)
    {
        //   gpio_set_level(GPIO_NUM_2, (count + 1) % 2);

        redSlidingWindow[slidingWindowWriteIndex] = 0;
        greenSlidingWindow[slidingWindowWriteIndex] = 0;
        blueSlidingWindow[slidingWindowWriteIndex] = 0;
        int audio = 0;
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
            for (int resultIndex = 0; resultIndex < 25; resultIndex++) {
                redSlidingWindow[slidingWindowWriteIndex] += adc1_get_raw(ADC1_CHANNEL_0);
                redSampleCount++;
                greenSlidingWindow[slidingWindowWriteIndex] += adc1_get_raw(ADC1_CHANNEL_3);
                greenSampleCount++;
                blueSlidingWindow[slidingWindowWriteIndex] += adc1_get_raw(ADC1_CHANNEL_6);
                blueSampleCount++;
            }
            audio += adc1_get_raw(ADC1_CHANNEL_7);
            audioSampleCount++;
            vTaskDelay(pdMS_TO_TICKS(10));
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
        ESP_LOGD(_logTag, "   %04d    %04d    %04d    %04d", redSampleCount, greenSampleCount, blueSampleCount, audioSampleCount);
        ESP_LOGD(_logTag, "   %04d    %04d    %04d    %04d    %d    %d    %d", redMovingAverage, greenMovingAverage, blueMovingAverage, audio, screenSwitchAtMcu, screenSwitchAtRelay, cameraEnable);

        count++;
    }
}

void AndroidAutoUnitAssist::CanReceiveTask(void* pvParameters) {
    AndroidAutoUnitAssist* androidAutoUnitAssist = (AndroidAutoUnitAssist*)pvParameters;
    twai_message_t message;
    while (true) {
        if (/*ESP_ERROR_CHECK_WITHOUT_ABORT*/(twai_receive(&message, pdMS_TO_TICKS(1000)/*portMAX_DELAY*/)) == ESP_OK) {
            if (!(message.rtr)) {
                xSemaphoreTake(androidAutoUnitAssist->_processingMutex, portMAX_DELAY);
                switch (message.data_length_code) {
                case 0:
                    ESP_LOGI(_logTag, "CAN RX: %03lX",
                        message.identifier
                    );
                    break;
                case 1:
                    ESP_LOGI(_logTag, "CAN RX: %03lX: %02X",
                        message.identifier,
                        message.data[0]
                    );
                    break;
                case 2:
                    ESP_LOGI(_logTag, "CAN RX: %03lX: %02X %02X",
                        message.identifier,
                        message.data[0],
                        message.data[1]
                    );
                    break;
                case 3:
                    ESP_LOGI(_logTag, "CAN RX: %03lX: %02X %02X %02X",
                        message.identifier,
                        message.data[0],
                        message.data[1],
                        message.data[2]
                    );
                    break;
                case 4:
                    ESP_LOGI(_logTag, "CAN RX: %03lX: %02X %02X %02X %02X",
                        message.identifier,
                        message.data[0],
                        message.data[1],
                        message.data[2],
                        message.data[3]
                    );
                    break;
                case 5:
                    ESP_LOGI(_logTag, "CAN RX: %03lX: %02X %02X %02X %02X %02X",
                        message.identifier,
                        message.data[0],
                        message.data[1],
                        message.data[2],
                        message.data[3],
                        message.data[4]
                    );
                    break;
                case 6:
                    ESP_LOGI(_logTag, "CAN RX: %03lX: %02X %02X %02X %02X %02X %02X",
                        message.identifier,
                        message.data[0],
                        message.data[1],
                        message.data[2],
                        message.data[3],
                        message.data[4],
                        message.data[5]
                    );
                    break;
                case 7:
                    ESP_LOGI(_logTag, "CAN RX: %03lX: %02X %02X %02X %02X %02X %02X %02X",
                        message.identifier,
                        message.data[0],
                        message.data[1],
                        message.data[2],
                        message.data[3],
                        message.data[4],
                        message.data[5],
                        message.data[6]
                    );
                    break;
                case 8:
                    ESP_LOGI(_logTag, "CAN RX: %03lX: %02X %02X %02X %02X %02X %02X %02X %02X",
                        message.identifier,
                        message.data[0],
                        message.data[1],
                        message.data[2],
                        message.data[3],
                        message.data[4],
                        message.data[5],
                        message.data[6],
                        message.data[7]
                    );
                    break;
                }
                xSemaphoreGive(androidAutoUnitAssist->_processingMutex);
            }
        }
        else {
            ESP_LOGD(_logTag, "CanReceiveTask: annoying log to show nothing has been received");
        }
    }
}