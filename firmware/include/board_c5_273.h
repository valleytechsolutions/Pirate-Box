#pragma once
#ifdef BOARD_WS_C5_273
#include <Arduino.h>
#include "c5_input.h"

struct C5SensorStatus {
  bool weatherValid=false,imuValid=false,expanderReady=false;
  float temperatureC=0,humidityPercent=0;
  float accelerationG[3]{},rotationDps[3]{};
  int batteryMillivolts=-1;
  uint32_t weatherAt=0,imuAt=0,weatherErrors=0,imuErrors=0,inputErrors=0;
};
// Sensors are sampled in boardLoop; rendering never waits for a measurement.
C5SensorStatus boardC5Sensors();
bool boardReadWeather(float& temperatureC,float& humidityPercent);
bool boardC5Command(const String& command);
piratebox::InputConfig boardC5InputConfig();
bool boardC5SetInput(const piratebox::InputConfig& config);
#endif
