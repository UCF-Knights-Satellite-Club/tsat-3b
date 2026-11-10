#include "ParafoilSim.h"
#include "Config.h"

void ParafoilSim::reset(const SimParams& p, const LocalFrame& frame) {
  p_ = p;
  frame_ = frame;
  rng_ = p.seed ? p.seed : 1;
  t_ = 0;
  pos_ = p.releasePos;
  vel_ = Vec2(0, 0);
  h_ = p.releaseHeight;
  vz_ = 0;
  psi_ = rand01() * 360.0f;
  r_ = 0;
  lineL_ = lineR_ = 0;
  dropT_ = -1;
  ground_ = false;
  tdSpeed_ = 0;
  nextGps_ = 0;
  gpsErr_ = Vec2(0, 0);
}

float ParafoilSim::rand01() {
  rng_ ^= rng_ << 13;
  rng_ ^= rng_ >> 17;
  rng_ ^= rng_ << 5;
  return (rng_ & 0xFFFFFF) / 16777216.0f;
}

float ParafoilSim::gauss() {
  float u1 = fmaxf(rand01(), 1e-6f), u2 = rand01();
  return sqrtf(-2.0f * logf(u1)) * cosf(2.0f * PI_F * u2);
}

Vec2 ParafoilSim::windAt(float h) const {
  // stronger higher up (power-law profile), plus slow gusts
  float k = powf(fmaxf(h, 1.0f) / 10.0f, 0.14f);
  float g = p_.gust * (sinf(t_ * 0.37f) + 0.5f * sinf(t_ * 1.3f + 1.0f));
  return p_.wind10m * k + Vec2(g, 0.6f * g);
}

void ParafoilSim::step(const WinchCommand& cmd, float dt) {
  t_ += dt;
  if (ground_) return;

  // Winches move at a limited speed
  float maxStep = p_.lineSpeed * dt;
  lineL_ += clampf(cmd.left - lineL_, -maxStep, maxStep);
  lineR_ += clampf(cmd.right - lineR_, -maxStep, maxStep);

  // Hanging under the carrier until the drop
  if (t_ < p_.holdTime) return;
  if (dropT_ < 0) dropT_ = t_;
  float sinceDrop = t_ - dropT_;

  if (sinceDrop < 1.2f) {
    // Free fall while the canopy opens
    vz_ = fmaxf(vz_ - 9.8f * dt, -12.0f);
    vel_ = vel_ * 0.98f + windAt(h_) * 0.02f;
  } else {
    float brake = 0.5f * (lineL_ + lineR_);
    float diff = (lineR_ - lineL_) / STEER_PULL;  // 1.0 = full steering
    float rTarget = clampf(diff, -1, 1) * p_.maxTurnRate + p_.turnBias;
    r_ += (rTarget - r_) * dt / 0.7f;
    psi_ = wrap360(psi_ + r_ * dt);

    float va = p_.airspeed * (1.0f - 0.25f * brake);
    float turnFrac = r_ / p_.maxTurnRate;
    float sink = p_.sink * (1.0f + 0.8f * brake * brake) * (1.0f + 0.6f * turnFrac * turnFrac);
    // canopy opening: blend from free fall into steady glide over ~1 s
    float blend = clampf((sinceDrop - 1.2f) / 1.0f, 0, 1);
    Vec2 vTarget = Vec2::fromBearing(psi_, va) + windAt(h_);
    vel_ = vel_ + (vTarget - vel_) * clampf(dt * (0.5f + 2.5f * blend), 0, 1);
    vz_ += (-sink - vz_) * clampf(dt * 2.0f, 0, 1);
  }

  pos_ = pos_ + vel_ * dt;
  h_ += vz_ * dt;
  if (h_ <= 0) {
    h_ = 0;
    ground_ = true;
    tdSpeed_ = sqrtf(vel_.dot(vel_) + vz_ * vz_);
    vel_ = Vec2(0, 0);
    vz_ = 0;
    r_ = 0;
  }
}

void ParafoilSim::fillSensors(SensorData& s) {
  s.t = t_;
  // Barometer: noisy, 50 Hz
  s.altValid = true;
  s.altitude = h_ + TARGET_ELEVATION_M + 0.3f * gauss();

  // IMU: compass with bias and noise
  s.imuValid = true;
  s.yawDeg = wrap360(psi_ + p_.compassBias + 1.5f * gauss() - MAG_DECLINATION_DEG);
  s.yawRateDps = r_ + 0.5f * gauss();

  // GPS: 5 Hz, slowly wandering position error
  s.gpsNew = false;
  if (t_ >= nextGps_) {
    nextGps_ = t_ + 0.2f;
    gpsErr_ = gpsErr_ * 0.98f + Vec2(gauss(), gauss()) * 0.15f;
    Vec2 p = pos_ + gpsErr_ + Vec2(gauss(), gauss()) * 0.4f;
    frame_.toGeo(p, s.lat, s.lon);
    s.velN = vel_.n + 0.15f * gauss();
    s.velE = vel_.e + 0.15f * gauss();
    s.gpsFix = true;
    s.gpsNew = true;
  }
  s.deployReleased = false;
}
