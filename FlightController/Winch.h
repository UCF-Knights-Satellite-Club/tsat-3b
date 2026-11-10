#pragma once
#include <Arduino.h>

/*
 * One winch: a sail-winch servo turning a helical-groove spool.
 *
 * setPull(0..1) is the fraction of LINE_PULL_MAX_MM to reel in.
 *   line pulled (mm) -> spool turns (mm / (pi * diameter)) -> servo pulse
 *
 * The helical groove keeps the line in a single layer, so every turn pulls
 * the same length and this mapping stays linear over the whole travel.
 */
class Winch {
 public:
  Winch(int pin, bool reversed) : pin_(pin), reversed_(reversed) {}
  void begin();
  void setPull(float fraction);
  float pull() const { return pull_; }
  int pulseUs() const { return us_; }

 private:
  int pin_;
  bool reversed_;
  float pull_ = 0;
  int us_ = 0;
#if ESP_ARDUINO_VERSION_MAJOR < 3
  static int nextChannel_;
  int channel_ = -1;
#endif
};
