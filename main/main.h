#pragma once

#include <driver/gpio.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

class Processing
{
public:
    Processing(gpio_num_t canTxPin, gpio_num_t canRxPin);
    void Start();

private:
    static void CanReceiveTask(void* pvParameters);
    // Event types
    enum class EventType
    {
        timerExpiry,
        screenSwitchedToIdrive,
        screenSwitchedToUnit,
        cameraEnabled,
        cameraDisabled,
    };

    const gpio_num_t _canTxPin;
    const gpio_num_t _canRxPin;
    const SemaphoreHandle_t _processingMutex;
    static constexpr const char* _logTag = "Processing";
};