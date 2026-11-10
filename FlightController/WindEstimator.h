#pragma once
#include "GeoMath.h"

/*
 * Estimates the wind and the parafoil's airspeed from GPS ground velocity and
 * the compass heading.
 *
 *   ground velocity = airspeed * (unit vector of heading) + wind
 *
 * Three unknowns (airspeed, wind N, wind E) are solved with recursive least
 * squares. They only become observable once the heading has changed by a
 * good amount, which the turns and zig-zag reversals provide. Old data is
 * forgotten over WIND_MEMORY_S so a changing wind is followed.
 */
class WindEstimator {
 public:
  void reset(float airspeedPrior);
  // Call once per new GPS velocity. headingDeg is true heading (declination applied).
  void update(float velN, float velE, float headingDeg, float dt);

  bool valid() const;
  float airspeed() const { return th_[0]; }
  Vec2 wind() const { return valid() ? Vec2(th_[1], th_[2]) : Vec2(0, 0); }

  // Ground speed we would make flying a ground track of courseDeg.
  // Returns <= 0 when the wind is too strong to make progress that way.
  // windScale scales the estimated wind (for wind shear at another height).
  float groundSpeedAlong(float courseDeg, float windScale = 1.0f) const;
  // Heading to fly (crabbed into the wind) to hold a ground track of courseDeg.
  float headingForCourse(float courseDeg) const;

 private:
  void scalarUpdate(const float phi[3], float y, float lambda);
  float th_[3] = {7, 0, 0};
  float P_[3][3] = {};
  int samples_ = 0;
  bool valid_ = false;
};
