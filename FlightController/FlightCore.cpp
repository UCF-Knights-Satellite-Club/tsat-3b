#include "FlightCore.h"
#include "Mission.h"

void FlightCore::begin() {
  frame_.setOrigin(TARGET.lat, TARGET.lon);
  guidance_.begin(frame_);
  altFilter_.reset();
  pid_.reset();
  nav_ = NavState();
  cmd_ = GuidanceCommand();
  out_ = WinchCommand();
  lastGpsT_ = -100;
  steer_ = headingErr_ = 0;
}

WinchCommand FlightCore::step(const SensorData& s, float dt) {
  // ---- Build the navigation state ----
  if (s.altValid) altFilter_.update(s.altitude, dt);
  nav_.t = s.t;
  nav_.height = altFilter_.altitude() - TARGET_ELEVATION_M;
  nav_.vz = altFilter_.rate();

  nav_.gpsNew = s.gpsFix && s.gpsNew;
  if (nav_.gpsNew) {
    lastGpsT_ = s.t;
    nav_.pos = frame_.toLocal(s.lat, s.lon);
    nav_.vel = Vec2(s.velN, s.velE);
  }
  nav_.gpsOk = s.gpsFix && (s.t - lastGpsT_) < GPS_TIMEOUT_S;
  nav_.headingOk = s.imuValid;
  nav_.headingDeg = wrap360(s.yawDeg + MAG_DECLINATION_DEG);
  nav_.yawRateDps = s.imuValid ? s.yawRateDps : 0.0f;
  nav_.deployReleased = s.deployReleased;

  // ---- Where to go ----
  cmd_ = guidance_.update(nav_, dt);

  // ---- Heading loop ----
  switch (cmd_.mode) {
    case GuidanceCommand::NEUTRAL:
      steer_ = 0;
      headingErr_ = 0;
      pid_.reset();
      break;

    case GuidanceCommand::DIRECT:
      steer_ = clampf(cmd_.steer, -1, 1);
      headingErr_ = 0;
      pid_.reset();
      break;

    case GuidanceCommand::HEADING:
    case GuidanceCommand::COURSE: {
      bool useCourse = cmd_.mode == GuidanceCommand::COURSE;
      if (useCourse && nav_.vel.length() < GPS_MIN_SPEED_FOR_COURSE) {
        steer_ = 0;  // GPS course is meaningless when barely moving
        break;
      }
      float actual = useCourse ? nav_.vel.bearing() : nav_.headingDeg;
      float err = wrap180(cmd_.targetDeg - actual);
      // Zig-zag reversals must turn toward the clear side of the corridor,
      // even when the other way round is shorter.
      if (cmd_.turnDir != 0 && fabsf(err) > 90.0f && (err > 0) != (cmd_.turnDir > 0)) {
        err += 360.0f * cmd_.turnDir;
      }
      headingErr_ = err;
      steer_ = pid_.update(err, s.imuValid ? s.yawRateDps : 0.0f, dt);
      break;
    }
  }

  // ---- Mixer: +steer pulls the right line and lets out the left ----
  float base = TRIM_PULL + cmd_.symmetric;
  out_.left = clampf(base - steer_ * STEER_PULL, 0, 1);
  out_.right = clampf(base + steer_ * STEER_PULL, 0, 1);
  return out_;
}
