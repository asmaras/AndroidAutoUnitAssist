#include "esphome/core/log.h"
#include "android_auto_unit_assist_component.h"
#include "AndroidAutoUnitAssist.h"

namespace esphome {
    namespace android_auto_unit_assist_component {

        static const char* TAG = "android_auto_unit_assist_component.component";
        static AndroidAutoUnitAssist androidAutoUnitAssist;

        void AndroidAutoUnitAssistComponent::setup() {
            androidAutoUnitAssist.Setup();
        }

        void AndroidAutoUnitAssistComponent::loop() {
            androidAutoUnitAssist.Loop();
        }

        void AndroidAutoUnitAssistComponent::dump_config() {
            ESP_LOGCONFIG(TAG, "Android auto unit assist component");
        }

    }  // namespace android_auto_unit_assist_component
}  // namespace esphome