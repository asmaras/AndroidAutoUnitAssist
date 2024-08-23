#include "AndroidAutoUnitAssist.h"
#include <Arduino.h>
#include <driver/adc.h>
#include <driver/twai.h>
#ifdef USE_ESP32_FRAMEWORK_ARDUINO
// Enable ESP_LOGX macros for logging when built with ESPHome
#include "esphome/core/log.h"
using namespace esphome;
#else
// Note: to enable ESP_LOGX macros for logging when built with PlatformIO, add build flag -D CORE_DEBUG_LEVEL=ARDUHAL_LOG_LEVEL_DEBUG
#include <esp_log.h>
#endif
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>
#include <driver/gpio.h>

namespace Pins {
    // CAN
    constexpr gpio_num_t canTx = GPIO_NUM_21;
    constexpr gpio_num_t canRx = GPIO_NUM_22;
    // Analog inputs
    constexpr adc1_channel_t redAdc = ADC1_CHANNEL_0;
    constexpr adc1_channel_t greenAdc = ADC1_CHANNEL_3;
    constexpr adc1_channel_t blueAdc = ADC1_CHANNEL_6;
    constexpr adc1_channel_t audioAdc = ADC1_CHANNEL_7;
    // Digital inputs
    constexpr gpio_num_t screenSwitchSelection = GPIO_NUM_25;
    constexpr gpio_num_t screenSwitchControl = GPIO_NUM_26;
    constexpr gpio_num_t cameraEnable = GPIO_NUM_27;
};

AndroidAutoUnitAssist::AndroidAutoUnitAssist() {
    _processingMutex = xSemaphoreCreateMutex();
}

void AndroidAutoUnitAssist::Setup() {
    // Configure the analog inputs
    adc1_config_width(ADC_WIDTH_BIT_12);
    adc1_config_channel_atten(Pins::redAdc, ADC_ATTEN_DB_0);
    adc1_config_channel_atten(Pins::greenAdc, ADC_ATTEN_DB_0);
    adc1_config_channel_atten(Pins::blueAdc, ADC_ATTEN_DB_0);
    adc1_config_channel_atten(Pins::audioAdc, ADC_ATTEN_DB_6);

    // Configure the digital I/Os
    gpio_config_t gpioConfig = {};
    gpioConfig.pin_bit_mask = 1ULL << Pins::screenSwitchSelection;
    gpioConfig.mode = GPIO_MODE_INPUT;
    gpio_config(&gpioConfig);
    gpioConfig.pin_bit_mask = 1ULL << Pins::cameraEnable;
    gpioConfig.mode = GPIO_MODE_INPUT;
    gpio_config(&gpioConfig);
    // The screenSwitchControl pin may only be an output if we need to intervene
    // Default we set it as an input
    gpioConfig.pin_bit_mask = 1ULL << Pins::screenSwitchControl;
    gpioConfig.mode = GPIO_MODE_INPUT;
    gpio_config(&gpioConfig);

    xTaskCreate(
        ReadAnalogInputsTask,
        "ReadAnalog",
        4096,
        this,
        configMAX_PRIORITIES / 2,
        nullptr
    );

    xTaskCreate(
        ReadDigitalInputsTask,
        "DetectUsrScrSw",
        4096,
        this,
        configMAX_PRIORITIES / 2,
        nullptr
    );

    twai_general_config_t generalConfig = TWAI_GENERAL_CONFIG_DEFAULT(Pins::canTx, Pins::canRx, TWAI_MODE_NORMAL);
    twai_timing_config_t timingConfig = TWAI_TIMING_CONFIG_100KBITS();
    twai_filter_config_t filterConfig = TWAI_FILTER_CONFIG_ACCEPT_ALL();

    ESP_ERROR_CHECK(twai_driver_install(&generalConfig, &timingConfig, &filterConfig));
    ESP_LOGI(_logTag, "Installed TWAI driver");

    ESP_ERROR_CHECK(twai_start());
    ESP_LOGI(_logTag, "Started TWAI driver");

    xTaskCreate(
        CanReceiveTask,
        "CanReceive",
        4096,
        this,
        configMAX_PRIORITIES / 2,
        nullptr
    );

    xTaskCreate(
        ScreenSelectionCorrectionTask,
        "ScreenSelCorr",
        4096,
        this,
        configMAX_PRIORITIES / 2,
        nullptr
    );

    xTaskCreate(
        CameraScreenControlOverruleTask,
        "CamScreenCtlOvr",
        4096,
        this,
        configMAX_PRIORITIES / 2,
        nullptr
    );
}

void AndroidAutoUnitAssist::Loop() {
    
}

void AndroidAutoUnitAssist::ReadAnalogInputsTask(void* pvParameters) {
    ((AndroidAutoUnitAssist*)pvParameters)->ReadAnalogInputsTask();
}

void AndroidAutoUnitAssist::ReadAnalogInputsTask() {
    constexpr int numberOfMeasurementsPerCycle = 30;
    constexpr int displaySamplesPerMeasurement = 25;

    TickType_t lastLogTime = xTaskGetTickCount();
    while (true)
    {
        int redSampleAccumulator = 0;
        int greenSampleAccumulator = 0;
        int blueSampleAccumulator = 0;
        int audioSampleAccumulator = 0;
        for (int measurementCount = 0; measurementCount < numberOfMeasurementsPerCycle; measurementCount++)
        {
            for (int resultIndex = 0; resultIndex < displaySamplesPerMeasurement; resultIndex++) {
                redSampleAccumulator += adc1_get_raw(Pins::redAdc);
                greenSampleAccumulator += adc1_get_raw(Pins::greenAdc);
                blueSampleAccumulator += adc1_get_raw(Pins::blueAdc);
            }
            audioSampleAccumulator += adc1_get_raw(Pins::audioAdc);
            vTaskDelay(pdMS_TO_TICKS(10));
        }
        int redSampleAverage = redSampleAccumulator / (numberOfMeasurementsPerCycle * displaySamplesPerMeasurement);
        int greenSampleAverage = greenSampleAccumulator / (numberOfMeasurementsPerCycle * displaySamplesPerMeasurement);
        int blueSampleAverage = blueSampleAccumulator / (numberOfMeasurementsPerCycle * displaySamplesPerMeasurement);
        audioSampleAccumulator /= numberOfMeasurementsPerCycle;

        TickType_t currentTime = xTaskGetTickCount();
        if (pdTICKS_TO_MS(currentTime - lastLogTime) >= 10000) {
            lastLogTime = currentTime;
            ESP_LOGD(
                _logTag, "R:%04d G:%04d B:%04d A:%04d",
                redSampleAverage, greenSampleAverage, blueSampleAverage, audioSampleAccumulator
            );
        }
    }
}

void AndroidAutoUnitAssist::ReadDigitalInputsTask(void* pvParameters) {
    ((AndroidAutoUnitAssist*)pvParameters)->ReadDigitalInputsTask();
}

void AndroidAutoUnitAssist::ReadDigitalInputsTask() {
    bool previousUnitScreenSelected = false;
    bool previousCameraEnabled = false;
    while (true) {
        xSemaphoreTake(_processingMutex, portMAX_DELAY);
        _unitScreenSelected = gpio_get_level(Pins::screenSwitchSelection) == 1;
        _cameraEnabled = gpio_get_level(Pins::cameraEnable) == 1;

        if (_unitScreenSelected != previousUnitScreenSelected) {
            ESP_LOGI(_logTag, "Screen switched to %s", _unitScreenSelected ? "unit" : "iDrive");
        }
        if (_cameraEnabled != previousCameraEnabled) {
            ESP_LOGI(_logTag, "Camera is %s", _cameraEnabled ? "enabled" : "disabled");
        }

        // Check if the screen switches to iDrive while the home button is pressed
        if (!_unitScreenSelected && previousUnitScreenSelected && _canCoder._iDriveController.homeButton) {
            _userSwitchedToIDriveScreen = true;
            ESP_LOGI(_logTag, "User switched to iDrive screen");
        }
        else if (_unitScreenSelected) {
            _userSwitchedToIDriveScreen = false;
        }

        previousUnitScreenSelected = _unitScreenSelected;
        previousCameraEnabled = _cameraEnabled;

        xSemaphoreGive(_processingMutex);
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

void AndroidAutoUnitAssist::CanReceiveTask(void* pvParameters) {
    AndroidAutoUnitAssist* androidAutoUnitAssist = (AndroidAutoUnitAssist*)pvParameters;
    twai_message_t message;
    while (true) {
        if (ESP_ERROR_CHECK_WITHOUT_ABORT(twai_receive(&message, portMAX_DELAY)) == ESP_OK) {
            if (!(message.rtr)) {
                xSemaphoreTake(androidAutoUnitAssist->_processingMutex, portMAX_DELAY);
                androidAutoUnitAssist->HandleCanMessage(message);
                xSemaphoreGive(androidAutoUnitAssist->_processingMutex);
            }
        }
    }
}

void AndroidAutoUnitAssist::HandleCanMessage(twai_message_t& message) {
    if (!_canCoder.Decode(message.identifier, message.data_length_code, message.data)) {
        return;
    }
    switch (_canCoder._identifier) {
    case CanCoder::Identifier::iDriveControler:
        ESP_LOGD(_logTag, "CAN RX: %s", _canCoder.RawMessageToString(message.identifier, message.data_length_code, message.data).c_str());
        ESP_LOGD(_logTag, "=> %s", _canCoder.ToString().c_str());
        ESP_LOGD(_logTag, "  _iDriveControllerLastDialValue=%d, _canCoder._iDriveController.dialValue=%d, delta=%d", _iDriveControllerLastDialValue, _canCoder._iDriveController.dialValue, _canCoder._iDriveController.dialValue - _iDriveControllerLastDialValue);

        // Correct for dial value wrap around flaw
        if (_unitScreenSelected) {
            if (_iDriveControllerLastDialValue == 0 && _canCoder._iDriveController.dialValue == 0xffff) {
                // The unit does not respond when the dial value wraps around from 0 to 0xffff
                // We fix this by sending the sequence [1, 0, 0xffff]
                // Sending 1 wraps the value around in the opposite direction and the unit doesn't respond. Then we send 0
                // which causes the unit to detect a left turn of the dial (what should have happened in the first place).
                // Last, we send 0xffff which repeats the original wrap around. There is no response of course, but we now
                // are at dial value 0xffff again and we made the unit detect one left turn. Further left turns will be
                // correctly detected.
                ESP_LOGI(_logTag, "  Dial value wraps around to 0xffff while showing unit's screen, applying downward correction");
                _canCoder._iDriveController.dialValue = 1;
                _canCoder.Encode(message.identifier, message.data_length_code, message.data);
                ESP_ERROR_CHECK_WITHOUT_ABORT(twai_transmit(&message, 0));
                ESP_LOGD(_logTag, "  CAN TX: %s", _canCoder.RawMessageToString(message.identifier, message.data_length_code, message.data).c_str());
                ESP_LOGD(_logTag, "  => %s", _canCoder.ToString().c_str());
                _canCoder._iDriveController.dialValue = 0;
                _canCoder.Encode(message.identifier, message.data_length_code, message.data);
                ESP_ERROR_CHECK_WITHOUT_ABORT(twai_transmit(&message, 0));
                ESP_LOGD(_logTag, "  CAN TX: %s", _canCoder.RawMessageToString(message.identifier, message.data_length_code, message.data).c_str());
                ESP_LOGD(_logTag, "  => %s", _canCoder.ToString().c_str());
                _canCoder._iDriveController.dialValue = 0xffff;
                _canCoder.Encode(message.identifier, message.data_length_code, message.data);
                ESP_ERROR_CHECK_WITHOUT_ABORT(twai_transmit(&message, 0));
                ESP_LOGD(_logTag, "  CAN TX: %s", _canCoder.RawMessageToString(message.identifier, message.data_length_code, message.data).c_str());
                ESP_LOGD(_logTag, "  => %s", _canCoder.ToString().c_str());
            }
            else if (_iDriveControllerLastDialValue == 0xffff && _canCoder._iDriveController.dialValue == 0) {
                // The unit does not respond when the dial value wraps around from 0xffff to 0
                // We fix this by sending the sequence [0xfffe, 0xffff, 0]
                // Sending 0xfffe wraps the value around in the opposite direction and the unit doesn't respond. Then we send
                // 0xffff which causes the unit to detect a right turn of the dial (what should have happened in the first
                // place). Last, we send 0 which repeats the original wrap around. There is no response of course, but we now
                // are at dial value 0 again and we made the unit detect one right turn. Further right turns will be
                // correctly detected.
                ESP_LOGI(_logTag, "  Dial value wraps around to 0 while showing unit's screen, applying upward correction");
                _canCoder._iDriveController.dialValue = 0xfffe;
                _canCoder.Encode(message.identifier, message.data_length_code, message.data);
                ESP_ERROR_CHECK_WITHOUT_ABORT(twai_transmit(&message, 0));
                ESP_LOGD(_logTag, "  CAN TX: %s", _canCoder.RawMessageToString(message.identifier, message.data_length_code, message.data).c_str());
                ESP_LOGD(_logTag, "  => %s", _canCoder.ToString().c_str());
                _canCoder._iDriveController.dialValue = 0xffff;
                _canCoder.Encode(message.identifier, message.data_length_code, message.data);
                ESP_ERROR_CHECK_WITHOUT_ABORT(twai_transmit(&message, 0));
                ESP_LOGD(_logTag, "  CAN TX: %s", _canCoder.RawMessageToString(message.identifier, message.data_length_code, message.data).c_str());
                ESP_LOGD(_logTag, "  => %s", _canCoder.ToString().c_str());
                _canCoder._iDriveController.dialValue = 0;
                _canCoder.Encode(message.identifier, message.data_length_code, message.data);
                ESP_ERROR_CHECK_WITHOUT_ABORT(twai_transmit(&message, 0));
                ESP_LOGD(_logTag, "  CAN TX: %s", _canCoder.RawMessageToString(message.identifier, message.data_length_code, message.data).c_str());
                ESP_LOGD(_logTag, "  => %s", _canCoder.ToString().c_str());
            }
        }
        _iDriveControllerLastDialValue = _canCoder._iDriveController.dialValue;
        break;
    default:
        break;
    }
}

void AndroidAutoUnitAssist::ScreenSelectionCorrectionTask(void* pvParameters) {
    ((AndroidAutoUnitAssist*)pvParameters)->ScreenSelectionCorrectionTask();
}

void AndroidAutoUnitAssist::ScreenSelectionCorrectionTask() {
    // Power-on delay
    vTaskDelay(pdMS_TO_TICKS(5000));

    twai_message_t canMessage;
    while (true)
    {
        // If the unit's screen is not shown and it wasn't caused by the user manually
        // switching to iDrive mode, send a long press of the home button to switch it back
        // The unit can be in two types of faulty states:
        // 1 - the screen is switched to iDrive and the unit is aware of this
        // 2 - the screen is switched to iDrive while the unit assumes it is displaying Android Auto or CarPlay
        // If state 2 is the case, a long press will change it to state 1. Then a second long press is
        // necessary switch the screen to the unit's content. The code below will keep trying until
        // the screen switches.
        xSemaphoreTake(_processingMutex, portMAX_DELAY);
        if (!_unitScreenSelected && !_userSwitchedToIDriveScreen) {
            ESP_LOGI(_logTag, "Sending home button long press to switch to the unit's screen");

            _canCoder._identifier = CanCoder::Identifier::iDriveControler;
            _canCoder._iDriveController.homeButton = true;
            _canCoder.Encode(canMessage.identifier, canMessage.data_length_code, canMessage.data);
            ESP_ERROR_CHECK_WITHOUT_ABORT(twai_transmit(&canMessage, 0));
            ESP_LOGD(_logTag, "CAN TX: %s", _canCoder.RawMessageToString(canMessage.identifier, canMessage.data_length_code, canMessage.data).c_str());
            ESP_LOGD(_logTag, "=> %s", _canCoder.ToString().c_str());

            xSemaphoreGive(_processingMutex);
            vTaskDelay(pdMS_TO_TICKS(2000));
            xSemaphoreTake(_processingMutex, portMAX_DELAY);

            _canCoder._identifier = CanCoder::Identifier::iDriveControler;
            _canCoder._iDriveController.homeButton = false;
            _canCoder.Encode(canMessage.identifier, canMessage.data_length_code, canMessage.data);
            ESP_ERROR_CHECK_WITHOUT_ABORT(twai_transmit(&canMessage, 0));
            ESP_LOGD(_logTag, "CAN TX: %s", _canCoder.RawMessageToString(canMessage.identifier, canMessage.data_length_code, canMessage.data).c_str());
            ESP_LOGD(_logTag, "=> %s", _canCoder.ToString().c_str());

            xSemaphoreGive(_processingMutex);
            vTaskDelay(pdMS_TO_TICKS(500));
        }
        else {
            xSemaphoreGive(_processingMutex);
            vTaskDelay(pdMS_TO_TICKS(10));
        }
    }
}

void AndroidAutoUnitAssist::CameraScreenControlOverruleTask(void* pvParameters) {
    ((AndroidAutoUnitAssist*)pvParameters)->CameraScreenControlOverruleTask();
}

void AndroidAutoUnitAssist::CameraScreenControlOverruleTask() {
    bool overruleActive = false;
    while (true) {
        // If the camera is enabled but the unit's screen is not shown, overrule the screen
        // switch control. The long press correction to switch the screen takes too long in
        // this situation. Until that has completed we overrule the switch control.
        xSemaphoreTake(_processingMutex, portMAX_DELAY);
        bool overruleRequired = _cameraEnabled && !_unitScreenSelected;
        xSemaphoreGive(_processingMutex);
        if (overruleRequired && !overruleActive) {
            ESP_LOGI(_logTag, "Camera enabled but iDrive screen selected. Overruling screen switch control to show unit's screen");
            gpio_config_t gpioConfig = {};
            gpioConfig.pin_bit_mask = 1ULL << Pins::screenSwitchControl;
            gpioConfig.mode = GPIO_MODE_OUTPUT;
            gpio_config(&gpioConfig);
            gpio_set_level(Pins::screenSwitchControl, 1);
            overruleActive = true;
        }
        else if (!overruleRequired && overruleActive) {
            ESP_LOGI(_logTag, "Screen switch overrule not required anymore");
            gpio_config_t gpioConfig = {};
            gpioConfig.pin_bit_mask = 1ULL << Pins::screenSwitchControl;
            gpioConfig.mode = GPIO_MODE_INPUT;
            gpio_config(&gpioConfig);
            overruleActive = false;
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}