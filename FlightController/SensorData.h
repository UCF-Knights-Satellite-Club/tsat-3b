#pragma once
// Everything the flight controller needs from the sensors, in one struct.
// Sensors.cpp fills it from the real hardware (or from the simulator).

struct SensorData {
  float t = 0;  // seconds since boot

  // GPS (after your Kalman filter)
  bool gpsFix = false;  // position is valid
  bool gpsNew = false;  // true only on the loop where a new GPS solution arrived
  double lat = 0, lon = 0;
  float velN = 0, velE = 0;  // ground velocity, m/s. From speed + course:
                             //   velN = speed * cos(course), velE = speed * sin(course)

  // Altitude (barometer), metres above the power-on ground level
  bool altValid = false;
  float altitude = 0;

  // IMU
  bool imuValid = false;
  float yawDeg = 0;      // magnetic heading, 0 = north, clockwise positive
  float pitchDeg = 0;
  float rollDeg = 0;
  float yawRateDps = 0;  // gyro yaw rate, positive = turning right (clockwise seen from above)

  bool deployReleased = false;  // optional breakwire / pull-pin
};
