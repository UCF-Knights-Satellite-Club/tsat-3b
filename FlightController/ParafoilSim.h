#pragma once
#include <stdint.h>
#include "FlightCore.h"
#include "GeoMath.h"
#include "SensorData.h"

/*
 * A simple parafoil simulator, used for bench tests on the ESP32
 * (SIMULATION 1 in Config.h) and by tools/pc_sim.cpp.
 *
 * It is deliberately a bit different from what the controller assumes
 * (airspeed, sink, turn bias, compass error, wind that changes with height)
 * so the estimators and the PID have something real to correct.
 */
struct SimParams {
  Vec2 releasePos = Vec2(300, -100);  // metres N/E of the target
  float releaseHeight = 150.0f;
  float holdTime = 3.0f;              // seconds hanging before the drop
  Vec2 wind10m = Vec2(-2.0f, 0.5f);   // wind velocity (blowing toward) at 10 m, m/s
  float gust = 0.6f;                  // gust amplitude, m/s
  float airspeed = 6.6f;              // true trim airspeed
  float sink = 1.3f;                  // true trim sink rate
  float maxTurnRate = 50.0f;          // deg/s at full differential pull
  float turnBias = 3.0f;              // deg/s the canopy turns on its own (right +)
  float compassBias = 4.0f;           // deg
  float lineSpeed = 0.7f;             // winch speed, fraction of full pull per second
  uint32_t seed = 1;
};

class ParafoilSim {
 public:
  void reset(const SimParams& p, const LocalFrame& frame);
  void step(const WinchCommand& cmd, float dt);
  void fillSensors(SensorData& s);

  float t() const { return t_; }
  Vec2 pos() const { return pos_; }
  float height() const { return h_; }
  float heading() const { return psi_; }
  bool onGround() const { return ground_; }
  float touchdownSpeed() const { return tdSpeed_; }
  Vec2 windAt(float h) const;

 private:
  float gauss();
  float rand01();

  SimParams p_;
  LocalFrame frame_;
  uint32_t rng_ = 1;
  float t_ = 0;
  Vec2 pos_, vel_;
  float h_ = 0, vz_ = 0;
  float psi_ = 0, r_ = 0;
  float lineL_ = 0, lineR_ = 0;
  float dropT_ = -1;
  bool ground_ = false;
  float tdSpeed_ = 0;
  float nextGps_ = 0;
  Vec2 gpsErr_;
};
