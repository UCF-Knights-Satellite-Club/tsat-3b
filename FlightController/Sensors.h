#pragma once
#include "SensorData.h"

// Hardware sensor glue. Wire your IMU, GPS and altitude code in Sensors.cpp.
void sensorsBegin();
void sensorsRead(SensorData& s);
