#pragma once
#include "Config.h"
#include "GeoMath.h"
#include "Guidance.h"
#include "HeadingController.h"
#include "SensorData.h"

// Line pull for each winch, 0 = released, 1 = LINE_PULL_MAX_MM
struct WinchCommand {
  float left = 0;
  float right = 0;
};

/*
 * FlightCore ties it together, once per loop:
 *   sensors -> navigation state -> Guidance -> heading PID -> line mixer
 * It has no Arduino code, so the PC simulator runs exactly the same logic.
 */
class FlightCore {
 public:
  void begin();
  WinchCommand step(const SensorData& s, float dt);
  void forceDeploy() { guidance_.forceDeploy(); }

  const Guidance& guidance() const { return guidance_; }
  const NavState& nav() const { return nav_; }
  const GuidanceCommand& command() const { return cmd_; }
  float steer() const { return steer_; }
  float headingError() const { return headingErr_; }
  const WinchCommand& winch() const { return out_; }
  const LocalFrame& frame() const { return frame_; }

 private:
  LocalFrame frame_;
  AltitudeFilter altFilter_;
  Guidance guidance_;
  HeadingController pid_{HEADING_KP, HEADING_KI, HEADING_KD, HEADING_I_LIMIT, HEADING_I_ZONE_DEG};
  NavState nav_;
  GuidanceCommand cmd_;
  WinchCommand out_;
  float lastGpsT_ = -100;
  float steer_ = 0;
  float headingErr_ = 0;
};
