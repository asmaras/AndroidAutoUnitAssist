#include <Arduino.h>
#include "AndroidAutoUnitAssist.h"

AndroidAutoUnitAssist androidAutoUnitAssist;

void setup() {
  androidAutoUnitAssist.Setup();
}

void loop() {
  androidAutoUnitAssist.Loop();
}