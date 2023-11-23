#include <driver/gpio.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

class AndroidAutoUnitAssist {
public:
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
    enum class TimerPeriod {
        timer1 = 1000,
        timer3 = 1000,
        timer4 = 1000,
        timer5 = 1000
    };
    enum class CpAAScreenActiveDetectionState {
        start,
        detectionActive
    } _cpAAScreenActiveDetectionState;
    void ProcessEvent(EventType eventType);
    void CpAAScreenActiveDetectionStateMachine(EventType eventType);
    void StartTimer(int timerNumber);
    static void ReadInputsTask(void* pvParameters);
    static void CanReceiveTask(void* pvParameters);

    struct Pins {
        // CAN
        static constexpr gpio_num_t canTx = GPIO_NUM_21;
        static constexpr gpio_num_t canRx = GPIO_NUM_22;
    } _pins;

    struct SamplingParameters {
        static constexpr int displayRefreshRate = 60;
        static constexpr int adcChannels = 4; // RGB + audio
        static constexpr int minimumSampleFrequency = SOC_ADC_SAMPLE_FREQ_THRES_LOW;
        // The ESP32 has an error that causes the actual sample frequency to be lower than expected
        // Compensate for this
        static constexpr double sampleFrequencyCorrectionFactor = 1.2226;
        // We want to take an equal number of samples per display frame for each channel,
        // so we need a sample frequency that produces a whole number of samples per display frame
        // If the minimum sample frequency doesn't produce this then we take one extra sample per
        // display frame and calculate the corresponding sample frequency
        // Note that the described error also causes a slightly different number of samples per
        // channel, so this is a best effort attempt
        static constexpr int singleDisplayChannelSamplesPerScreenFrame = minimumSampleFrequency % displayRefreshRate == 0 ? minimumSampleFrequency / displayRefreshRate : (minimumSampleFrequency / displayRefreshRate) + 1;
        static constexpr int sampleFrequency = singleDisplayChannelSamplesPerScreenFrame * adcChannels * displayRefreshRate * sampleFrequencyCorrectionFactor;
        // Per read operation we get samples for the display channels plus the audio channel
        static constexpr int samplesPerRead = singleDisplayChannelSamplesPerScreenFrame * adcChannels;
    } _samplingParameters;
    SemaphoreHandle_t _processingMutex;
    static constexpr const char* _logTag = "AndroidAutoUnitAssist";
    TickType_t _timer2StartTime;
};