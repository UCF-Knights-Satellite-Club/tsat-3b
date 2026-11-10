#include "WindEstimator.h"
#include "Config.h"

namespace {
constexpr float WIND_MEMORY_S = 60.0f;      // how long old measurements still count
constexpr float AIRSPEED_STD_VALID = 0.6f;  // estimate is trusted once airspeed is known this well (m/s)
constexpr int MIN_SAMPLES = 20;
constexpr float P_TRACE_MAX = 60.0f;        // stop forgetting when this uncertain (no heading change)
constexpr float WIND_MAX_MPS = 15.0f;
}  // namespace

void WindEstimator::reset(float airspeedPrior) {
  th_[0] = airspeedPrior;
  th_[1] = th_[2] = 0;
  for (int i = 0; i < 3; i++)
    for (int j = 0; j < 3; j++) P_[i][j] = 0;
  P_[0][0] = 2.0f * 2.0f;  // airspeed prior: +-2 m/s
  P_[1][1] = 5.0f * 5.0f;  // wind prior: +-5 m/s each axis
  P_[2][2] = 5.0f * 5.0f;
  samples_ = 0;
  valid_ = false;
}

void WindEstimator::scalarUpdate(const float phi[3], float y, float lambda) {
  float Pphi[3];
  for (int i = 0; i < 3; i++) Pphi[i] = P_[i][0] * phi[0] + P_[i][1] * phi[1] + P_[i][2] * phi[2];
  float denom = lambda + phi[0] * Pphi[0] + phi[1] * Pphi[1] + phi[2] * Pphi[2];
  float k[3] = {Pphi[0] / denom, Pphi[1] / denom, Pphi[2] / denom};
  float err = y - (phi[0] * th_[0] + phi[1] * th_[1] + phi[2] * th_[2]);
  for (int i = 0; i < 3; i++) th_[i] += k[i] * err;
  // P = (P - k * phi' * P) / lambda   (P is symmetric so phi' * P = Pphi')
  for (int i = 0; i < 3; i++)
    for (int j = 0; j < 3; j++) P_[i][j] = (P_[i][j] - k[i] * Pphi[j]) / lambda;
}

void WindEstimator::update(float velN, float velE, float headingDeg, float dt) {
  float trace = P_[0][0] + P_[1][1] + P_[2][2];
  float lambda = trace > P_TRACE_MAX ? 1.0f : expf(-dt / WIND_MEMORY_S);

  float c = cosf(headingDeg * DEG2RAD), s = sinf(headingDeg * DEG2RAD);
  const float phiN[3] = {c, 1, 0};  // velN = Va*cos(h) + Wn
  const float phiE[3] = {s, 0, 1};  // velE = Va*sin(h) + We
  scalarUpdate(phiN, velN, lambda);
  scalarUpdate(phiE, velE, 1.0f);

  th_[0] = clampf(th_[0], 0.5f * AIRSPEED_NOMINAL_MPS, 1.6f * AIRSPEED_NOMINAL_MPS);
  th_[1] = clampf(th_[1], -WIND_MAX_MPS, WIND_MAX_MPS);
  th_[2] = clampf(th_[2], -WIND_MAX_MPS, WIND_MAX_MPS);

  samples_++;
  if (samples_ >= MIN_SAMPLES && P_[0][0] < AIRSPEED_STD_VALID * AIRSPEED_STD_VALID) valid_ = true;
}

bool WindEstimator::valid() const { return valid_; }

float WindEstimator::groundSpeedAlong(float courseDeg, float windScale) const {
  Vec2 t = Vec2::fromBearing(courseDeg);
  Vec2 w = wind() * windScale;
  float va = valid_ ? th_[0] : AIRSPEED_NOMINAL_MPS;
  float wAlong = w.dot(t);
  float wCross = t.cross(w);  // wind component pushing us to the right of the track
  float under = va * va - wCross * wCross;
  if (under <= 0) return 0;  // crosswind stronger than airspeed: cannot hold this track
  return wAlong + sqrtf(under);
}

float WindEstimator::headingForCourse(float courseDeg) const {
  Vec2 t = Vec2::fromBearing(courseDeg);
  Vec2 w = wind();
  float va = valid_ ? th_[0] : AIRSPEED_NOMINAL_MPS;
  float wCross = t.cross(w);
  // crab into the wind: heading = course - asin(crosswind / airspeed)
  return wrap360(courseDeg - asinf(clampf(wCross / va, -0.95f, 0.95f)) * RAD2DEG);
}
