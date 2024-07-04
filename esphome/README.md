## Why use ESPHome?

ESPHome is a great way to have a provisioning framework in your ESP project. It provides WiFi provisioning and over-the-air updates. Firmware updates can be installed on devices using a local web page. Just use ESPHome to build the project and the firmware will contain all the provisioning code!

Using ESPHome as a plugin within Home Assistant is an easy way to setup the development environment. You don't have to actually create a device to use in Home Assistant, just use Home Assistant and ESPHome to build your project.

To build this project with ESPHome:
- In Home Assistant's ESPHome screen, create a device for the project
- Copy the contents of androidautounitassist.yaml (except key and passwords) to the device configuration
- Copy the "external_components" directory to the esphome/ directory on the Home Assistant machine
  \
  This contains a simple "external component" to jump to the actual application code. It is registered to ESPHome through the \_\_init\_\_.py file. The YAML file contains lines to actually use it when the project is being built
- Copy the source code to esphome/external_components/android_auto_unit_assist_component/

To enable use of the ESP_LOGX macros in the application when built with either ESPHome or PlatformIO, use the following in your code:
\
(also note the additional build flag)
```
#ifdef USE_ESP32_FRAMEWORK_ARDUINO
// Enable ESP_LOGX macros for logging when built with ESPHome
#include "esphome/core/log.h"
using namespace esphome;
#else
// Note: to enable ESP_LOGX macros for logging when built with PlatformIO, add build flag -D CORE_DEBUG_LEVEL=ARDUHAL_LOG_LEVEL_DEBUG
#include <esp_log.h>
#endif
```
