#include "Sensors.h"
#include <Arduino.h>
#include "Config.h"

/*
 * Connect your existing IMU, GPS (Kalman) and altitude code here.
 *
 * Until it is connected every sensor reports "not valid". The controller
 * then stays in PREFLIGHT with the lines neutral, so nothing moves
 * unexpectedly.
 *
 * Things to get right:
 *  - yawDeg: magnetic heading, 0 = north, increasing clockwise (turning
 *    right). If your IMU's yaw increases counter-clockwise, use 360 - yaw.
 *  - yawRateDps: gyro z rate in deg/s, positive turning right. This is the
 *    PID's damping term, so check the sign: a wrong sign makes it oscillate.
 *  - altitude: metres above the ground where the board was switched on.
 *    Zero it in sensorsBegin() from an average of a second or two of readings.
 *  - gpsNew: true ONLY on the loop where your GPS code produced a new
 *    solution. The wind estimator counts on it.
 *  - Do not block. This runs 50 times a second; read the GPS UART with
 *    whatever bytes are waiting and return.
 */

#if !SIMULATION
#warning "Sensors.cpp: hardware sensors are not connected yet - see the comments in this file"
#endif

void sensorsBegin() {
  if (PIN_DEPLOY_SWITCH >= 0) pinMode(PIN_DEPLOY_SWITCH, INPUT_PULLUP);

  // imuBegin();       // e.g. your IMU file's setup
  // gpsBegin();       // e.g. Serial2.begin(9600, SERIAL_8N1, RX, TX) + your Kalman init
  // altitudeBegin();  // e.g. barometer setup + zero at the current height
}

void sensorsRead(SensorData& s) {
  s.t = micros() * 1e-6f;

  // ---- IMU ----
  s.imuValid = false;
  // s.imuValid   = imuHasData();
  // s.yawDeg     = imuYaw();
  // s.pitchDeg   = imuPitch();
  // s.rollDeg    = imuRoll();
  // s.yawRateDps = imuGyroZ();

  // ---- GPS ----
  s.gpsNew = false;
  s.gpsFix = false;
  // if (gpsUpdate()) {            // returns true when a new filtered solution is ready
  //   s.gpsNew = true;
  //   s.gpsFix = gpsHasFix();
  //   s.lat = gpsLatitude();      // keep these as double, float loses ~1 m
  //   s.lon = gpsLongitude();
  //   float spd = gpsSpeedMps(), crs = gpsCourseDeg();
  //   s.velN = spd * cosf(crs * DEG_TO_RAD);
  //   s.velE = spd * sinf(crs * DEG_TO_RAD);
  // }

  // ---- Altitude ----
  s.altValid = false;
  // s.altValid = true;
  // s.altitude = altitudeMetres();

  s.deployReleased = PIN_DEPLOY_SWITCH >= 0 && digitalRead(PIN_DEPLOY_SWITCH) == HIGH;
}
