#pragma once

/*
 * Heading PID. Turns a heading error into a steering command in -1..+1
 * (+1 = full right turn = pull the right line).
 *
 * Compared with the first version of the loop:
 *  - the D term uses the measured yaw rate instead of differentiating the
 *    error, so a jump in the target heading (new waypoint) does not kick the lines
 *  - the integral is multiplied by ki and only builds up when the error is
 *    small and the output is not saturated (anti-windup)
 */
class HeadingController {
 public:
  HeadingController(float kp, float ki, float kd, float iLimit, float iZoneDeg)
      : kp_(kp), ki_(ki), kd_(kd), iLimit_(iLimit), iZone_(iZoneDeg) {}

  // errorDeg: desired - actual, already wrapped to [-180, 180)
  // yawRateDps: positive turning right
  float update(float errorDeg, float yawRateDps, float dt);
  void reset() { integral_ = 0; }

  float pTerm() const { return p_; }
  float iTerm() const { return integral_; }
  float dTerm() const { return d_; }

 private:
  float kp_, ki_, kd_, iLimit_, iZone_;
  float integral_ = 0;
  float p_ = 0, d_ = 0;
};
