#pragma once
// Small math helpers shared by everything. No Arduino dependencies so the
// guidance code also builds on a PC for simulation.

#include <math.h>

constexpr float DEG2RAD = 0.017453292f;
constexpr float RAD2DEG = 57.29577951f;

inline float clampf(float x, float lo, float hi) { return x < lo ? lo : (x > hi ? hi : x); }

// Wrap any angle into [-180, 180)
inline float wrap180(float deg) {
  deg = fmodf(deg + 180.0f, 360.0f);
  if (deg < 0) deg += 360.0f;
  return deg - 180.0f;
}

// Wrap any angle into [0, 360)
inline float wrap360(float deg) {
  deg = fmodf(deg, 360.0f);
  if (deg < 0) deg += 360.0f;
  return deg;
}

// Horizontal vector in metres, north/east. Bearings are degrees clockwise from north.
struct Vec2 {
  float n = 0;
  float e = 0;
  Vec2() = default;
  Vec2(float n_, float e_) : n(n_), e(e_) {}
  Vec2 operator+(const Vec2& o) const { return {n + o.n, e + o.e}; }
  Vec2 operator-(const Vec2& o) const { return {n - o.n, e - o.e}; }
  Vec2 operator*(float k) const { return {n * k, e * k}; }
  float dot(const Vec2& o) const { return n * o.n + e * o.e; }
  // positive when o is to the right of this vector
  float cross(const Vec2& o) const { return n * o.e - e * o.n; }
  float length() const { return sqrtf(n * n + e * e); }
  float bearing() const { return wrap360(atan2f(e, n) * RAD2DEG); }
  Vec2 unit() const {
    float l = length();
    return l > 1e-6f ? Vec2(n / l, e / l) : Vec2(1, 0);
  }
  static Vec2 fromBearing(float deg, float len = 1.0f) {
    return {cosf(deg * DEG2RAD) * len, sinf(deg * DEG2RAD) * len};
  }
};

// Flat-earth north/east metres around an origin. Accurate to well under a
// metre over a few km, which is far more than a parafoil flight covers.
class LocalFrame {
 public:
  void setOrigin(double lat0, double lon0) {
    lat0_ = lat0;
    lon0_ = lon0;
    mPerDegLon_ = M_PER_DEG_LAT * cos(lat0 * 0.017453292519943295);
  }
  Vec2 toLocal(double lat, double lon) const {
    return {(float)((lat - lat0_) * M_PER_DEG_LAT), (float)((lon - lon0_) * mPerDegLon_)};
  }
  void toGeo(const Vec2& p, double& lat, double& lon) const {
    lat = lat0_ + p.n / M_PER_DEG_LAT;
    lon = lon0_ + p.e / mPerDegLon_;
  }

 private:
  static constexpr double M_PER_DEG_LAT = 111195.0;
  double lat0_ = 0, lon0_ = 0, mPerDegLon_ = M_PER_DEG_LAT;
};

// First-order low-pass filter with a time constant in seconds
class LowPass {
 public:
  explicit LowPass(float tau = 1.0f) : tau_(tau) {}
  float update(float x, float dt) {
    if (!init_) {
      y_ = x;
      init_ = true;
    } else {
      y_ += (x - y_) * (dt / (tau_ + dt));
    }
    return y_;
  }
  void reset() { init_ = false; }
  float value() const { return y_; }
  bool ready() const { return init_; }

 private:
  float tau_;
  float y_ = 0;
  bool init_ = false;
};

// Alpha-beta filter: smooths a noisy altitude and gives its rate (m/s, up positive)
class AltitudeFilter {
 public:
  void update(float altMeas, float dt) {
    if (!init_) {
      alt_ = altMeas;
      rate_ = 0;
      init_ = true;
      return;
    }
    alt_ += rate_ * dt;
    float r = altMeas - alt_;
    alt_ += ALPHA * r;
    rate_ += (BETA / dt) * r;
  }
  float altitude() const { return alt_; }
  float rate() const { return rate_; }
  void reset() { init_ = false; }

 private:
  // Tuned for a 50 Hz barometer with ~0.3 m noise. Raise BETA for a faster
  // (noisier) climb rate, lower it for a smoother (slower) one.
  static constexpr float ALPHA = 0.05f;
  static constexpr float BETA = 0.0015f;
  float alt_ = 0, rate_ = 0;
  bool init_ = false;
};
