#include "Winch.h"
#include "Config.h"
#include "GeoMath.h"

namespace {
constexpr int PWM_BITS = 16;
constexpr float PERIOD_US = 1000000.0f / SERVO_PWM_HZ;
constexpr float MM_PER_TURN = PI_F * SPOOL_DIAMETER_MM;
}  // namespace

#if ESP_ARDUINO_VERSION_MAJOR < 3
int Winch::nextChannel_ = 0;
#endif

void Winch::begin() {
#if ESP_ARDUINO_VERSION_MAJOR >= 3
  ledcAttach(pin_, SERVO_PWM_HZ, PWM_BITS);
#else
  channel_ = nextChannel_++;
  ledcSetup(channel_, SERVO_PWM_HZ, PWM_BITS);
  ledcAttachPin(pin_, channel_);
#endif
  setPull(0);
}

void Winch::setPull(float fraction) {
  pull_ = clampf(fraction, 0, 1);
  float turns = pull_ * LINE_PULL_MAX_MM / MM_PER_TURN;
  float travel = turns / SERVO_TURNS_FULL_RANGE;  // 0..1 of servo range
  if (reversed_) travel = 1.0f - travel;
  us_ = (int)lroundf(SERVO_US_MIN + travel * (SERVO_US_MAX - SERVO_US_MIN));
  uint32_t duty = (uint32_t)(us_ / PERIOD_US * ((1u << PWM_BITS) - 1));
#if ESP_ARDUINO_VERSION_MAJOR >= 3
  ledcWrite(pin_, duty);
#else
  ledcWrite(channel_, duty);
#endif
}
