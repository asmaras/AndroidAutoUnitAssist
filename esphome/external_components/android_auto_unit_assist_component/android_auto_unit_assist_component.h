#pragma once

#include "esphome/core/component.h"

namespace esphome {
    namespace android_auto_unit_assist_component {

        class AndroidAutoUnitAssistComponent : public Component {
        public:
            void setup() override;
            void loop() override;
            void dump_config() override;
        };

    }  // namespace android_auto_unit_assist_component
}  // namespace esphome