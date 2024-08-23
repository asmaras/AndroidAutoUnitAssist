#include "CanCoder.h"
#include <driver/gpio.h>
#include <driver/twai.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

class AndroidAutoUnitAssist {
public:
    AndroidAutoUnitAssist();
    void Setup();
    void Loop();

private:
    // Event types
    enum class EventType
    {
        stateEntry,
        timer1Expiry,
        timer2Expiry,
        timer3Expiry,
        timer4Expiry,
        timer5Expiry,
        screenSwitched,
        cameraEnableChanged,
        screenTransitionDetected,
        menuScreenActiveChanged,
        newMenuScreenLevelDetected,
        audioDetected,
        homeButtonLongPressed
    };
    static void ReadAnalogInputsTask(void* pvParameters);
    void ReadAnalogInputsTask();
    static void ReadDigitalInputsTask(void* pvParameters);
    void ReadDigitalInputsTask();
    static void CanReceiveTask(void* pvParameters);
    void HandleCanMessage(twai_message_t& message);
    static void ScreenSelectionCorrectionTask(void* pvParameters);
    void ScreenSelectionCorrectionTask();
    static void CameraScreenControlOverruleTask(void* pvParameters);
    void CameraScreenControlOverruleTask();

    SemaphoreHandle_t _processingMutex;
    static constexpr const char* _logTag = "AndroidAutoUnitAssist";
    bool _unitScreenSelected = false;
    bool _cameraEnabled = false;
    CanCoder _canCoder;
    unsigned short _iDriveControllerLastDialValue = 0;
    bool _userSwitchedToIDriveScreen = false;
};