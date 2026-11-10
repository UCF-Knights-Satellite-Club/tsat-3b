#include "HeadingController.h"
#include "GeoMath.h"

float HeadingController::update(float errorDeg, float yawRateDps, float dt) {
  p_ = kp_ * errorDeg;
  // d(error)/dt = -yawRate while the target heading holds still
  d_ = -kd_ * yawRateDps;

  float unsat = p_ + integral_ + d_;
  bool saturated = unsat > 1.0f || unsat < -1.0f;
  bool pushingFurther = (unsat > 0) == (errorDeg > 0);
  if (fabsf(errorDeg) < iZone_ && !(saturated && pushingFurther)) {
    integral_ = clampf(integral_ + ki_ * errorDeg * dt, -iLimit_, iLimit_);
  }
  return clampf(p_ + integral_ + d_, -1.0f, 1.0f);
}
