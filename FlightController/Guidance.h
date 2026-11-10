#pragma once
#include <stdint.h>
#include "GeoMath.h"
#include "Mission.h"
#include "WindEstimator.h"

/*
 * Guidance: decides WHERE to go. Owns the flight phase state machine,
 * follows the waypoint path, decides when to zig-zag, and manages height on
 * the final approach. It outputs a desired course/heading plus a symmetric
 * brake, and FlightCore turns that into line pulls.
 *
 *   PREFLIGHT -> ARMED -> DEPLOY -> NAVIGATE -> (ZIGZAG_FWD <-> ZIGZAG_BACK) -> NAVIGATE
 *             -> FINAL -> (ORBIT -> FINAL) -> FLARE -> LANDED
 *   SPIRAL: emergency (left the fence, or GPS lost too long)
 */

enum class Phase : uint8_t {
  PREFLIGHT,    // on the ground, waiting to be carried up
  ARMED,        // high enough, waiting for the drop
  DEPLOY,       // canopy opening, lines neutral
  NAVIGATE,     // following the waypoint path
  ZIGZAG_BACK,  // zig-zag: flying back along the corridor
  ZIGZAG_FWD,   // zig-zag: flying forward along the corridor again
  FINAL,        // final approach to the target, brakes control the glide
  ORBIT,        // overshot the target still high: circle it to lose height
  SPIRAL,       // emergency descent in place
  FLARE,        // just above the ground: both lines pulled
  LANDED
};
const char* phaseName(Phase p);

// What Guidance knows about the vehicle, built by FlightCore every loop
struct NavState {
  float t = 0;
  bool gpsOk = false;     // fix is recent
  bool gpsNew = false;    // fresh fix this loop
  Vec2 pos;               // metres north/east of the target
  Vec2 vel;               // ground velocity, m/s
  float height = 0;       // metres above the target
  float vz = 0;           // vertical speed, up positive
  bool headingOk = false;
  float headingDeg = 0;   // true heading
  float yawRateDps = 0;   // positive turning right
  bool deployReleased = false;
};

struct GuidanceCommand {
  enum Mode : uint8_t {
    NEUTRAL,  // no steering
    HEADING,  // hold targetDeg as a compass heading (true north)
    COURSE,   // hold targetDeg as a GPS course over ground
    DIRECT    // use steer as-is
  };
  Mode mode = NEUTRAL;
  float targetDeg = 0;
  float steer = 0;       // DIRECT only, -1..+1
  float symmetric = 0;   // extra pull on both lines, 0..1 (brakes / flare)
  int8_t turnDir = 0;    // +1/-1 forces the direction of a big turn (zig-zag reversals)
};

class Guidance {
 public:
  void begin(const LocalFrame& frame);  // frame origin must be the TARGET
  GuidanceCommand update(const NavState& s, float dt);
  void forceDeploy();  // bench testing: skip the drop detector

  // Telemetry
  Phase phase() const { return phase_; }
  float desiredCourse() const { return desiredCourse_; }
  float crossTrack() const { return crossTrack_; }
  float excessHeight() const { return excess_; }
  int targetWaypoint() const { return toIdx_; }
  int zigzagCount() const { return zigzags_; }
  float sinkRate() const;
  float turnLoss() const { return turnLoss_; }
  const WindEstimator& wind() const { return wind_; }
  float timeInPhase(float t) const { return t - phaseT0_; }

  // Ground glide ratio along a course at height h, from the wind and sink estimates
  float groundGlide(float courseDeg, float h) const;
  // Height needed to fly from 'from' to 'to' in a straight line, starting at hStart
  float heightToFly(const Vec2& from, const Vec2& to, float hStart) const;
  // Height needed to fly from 'from' through waypoints idx..target, starting
  // at hStart. arriveCourse (if >= 0) is the course we arrive at 'from' on,
  // so the corner there is charged too.
  float heightToFinish(const Vec2& from, int idx, float hStart, float arriveCourse = -1) const;
  float cornerLoss(float fromCourse, float toCourse) const;

 private:
  void setPhase(Phase p, float t);
  void startLeg(const Vec2& from, const Vec2& to) { legFrom_ = from; legTo_ = to; }
  float followLeg(const Vec2& pos, float& along, float& legLen);
  float orbitCourse(const Vec2& pos) const;
  float weave(float course, float along, float legLen);
  float weaveCapacity(float h) const;
  void onWaypointReached(const NavState& s);
  float planReversal(float height);
  float windDriftCost(float newCourse, float h) const;
  float reversalCost(float newCourse, float h) const;
  float windScale(float h) const;
  static Vec2 aimPast(const Vec2& from, const Vec2& target);
  static float acceptDistance(float legLen);
  void startReversal(const NavState& s, int8_t dir);
  bool inFlight() const;

  static constexpr int MAX_WP = PATH_COUNT + 1;
  Vec2 wp_[MAX_WP];  // PATH points then the target (0,0)
  int lastIdx_ = 0;  // index of the target in wp_
  Vec2 aimPoint_;    // glide planning point just past the target

  Phase phase_ = Phase::PREFLIGHT;
  float phaseT0_ = 0;
  Vec2 legFrom_, legTo_;
  int toIdx_ = 0;
  int zigzags_ = 0;
  Vec2 zzA_, zzB_;
  Vec2 zzOffset_;  // from the corridor line to the racetrack's back line
  float plannedSpare_ = 0;  // what the current zig-zag plan expects to have left
  bool forceDeploy_ = false;

  WindEstimator wind_;
  LowPass sink_{10.0f};
  LowPass windRefH_{30.0f};
  float dropTimer_ = 0;
  float landedTimer_ = 0;
  float lastGpsT_ = -100;
  float lastWindT_ = 0;
  float holdHeading_ = 0;
  bool holdHeadingValid_ = false;
  int8_t turnDir_ = 0;
  float turnLoss_ = 0;      // learned height lost per 180 deg reversal
  float turnStartH_ = 0;
  float turnStartT_ = 0;
  bool measuringTurn_ = false;

  float weaveDeg_ = 0;
  int8_t weaveSign_ = 1;

  float desiredCourse_ = 0;
  float crossTrack_ = 0;
  float excess_ = 0;
};
