#include "Guidance.h"
#include "Config.h"

const char* phaseName(Phase p) {
  switch (p) {
    case Phase::PREFLIGHT: return "PREFLIGHT";
    case Phase::ARMED: return "ARMED";
    case Phase::DEPLOY: return "DEPLOY";
    case Phase::NAVIGATE: return "NAVIGATE";
    case Phase::ZIGZAG_BACK: return "ZIGZAG_BACK";
    case Phase::ZIGZAG_FWD: return "ZIGZAG_FWD";
    case Phase::FINAL: return "FINAL";
    case Phase::ORBIT: return "ORBIT";
    case Phase::SPIRAL: return "SPIRAL";
    case Phase::FLARE: return "FLARE";
    case Phase::LANDED: return "LANDED";
  }
  return "?";
}

// The zig-zag is flown as a racetrack: forward on the corridor line, turn,
// back on a parallel line ZIGZAG_SPACING_M to the clear side, turn the same
// way again onto the corridor line. Both turns go the same direction, so the
// parafoil never crosses to the other side of the corridor.
static int8_t reversalTurn() { return ZIGZAG_SIDE_RIGHT ? +1 : -1; }
static constexpr float MIN_ZIGZAG_LEG_S = 2.0f;  // a zig-zag leg never ends sooner than this

void Guidance::begin(const LocalFrame& frame) {
  for (int i = 0; i < PATH_COUNT; i++) wp_[i] = frame.toLocal(PATH[i].lat, PATH[i].lon);
  lastIdx_ = PATH_COUNT;
  wp_[lastIdx_] = Vec2(0, 0);  // the frame origin is the target
  aimPoint_ = aimPast(wp_[lastIdx_ - 1], wp_[lastIdx_]);
  if (ZIGZAG_LEG >= 0) {
    zzA_ = wp_[ZIGZAG_LEG];
    zzB_ = wp_[ZIGZAG_LEG + 1];
    Vec2 t = (zzB_ - zzA_).unit();
    Vec2 right(-t.e, t.n);
    zzOffset_ = right * (ZIGZAG_SIDE_RIGHT ? ZIGZAG_SPACING_M : -ZIGZAG_SPACING_M);
  }
  wind_.reset(AIRSPEED_NOMINAL_MPS);
  sink_.reset();
  windRefH_.reset();
  phase_ = Phase::PREFLIGHT;
  phaseT0_ = 0;
  toIdx_ = 0;
  zigzags_ = 0;
  dropTimer_ = landedTimer_ = 0;
  lastGpsT_ = -100;
  lastWindT_ = 0;
  holdHeadingValid_ = false;
  turnDir_ = 0;
  turnLoss_ = TURN_180_ALT_LOSS_M;
  measuringTurn_ = false;
  forceDeploy_ = false;
}

void Guidance::forceDeploy() { forceDeploy_ = true; }

// Point the glide is planned to: past the target along the approach
// direction, because the flare at the end makes the parafoil land short.
Vec2 Guidance::aimPast(const Vec2& from, const Vec2& target) {
  return target + (target - from).unit() * FLARE_AIM_PAST_M;
}

void Guidance::setPhase(Phase p, float t) {
  phase_ = p;
  phaseT0_ = t;
  turnDir_ = 0;
}

bool Guidance::inFlight() const {
  return phase_ == Phase::NAVIGATE || phase_ == Phase::ZIGZAG_BACK || phase_ == Phase::ZIGZAG_FWD ||
         phase_ == Phase::FINAL || phase_ == Phase::ORBIT || phase_ == Phase::SPIRAL;
}

float Guidance::sinkRate() const {
  return sink_.ready() ? clampf(sink_.value(), 0.4f, 6.0f) : SINK_RATE_NOMINAL_MPS;
}

float Guidance::windScale(float h) const {
  if (WIND_SHEAR_EXPONENT <= 0 || !windRefH_.ready()) return 1.0f;
  return powf(fmaxf(h, 2.0f) / fmaxf(windRefH_.value(), 2.0f), WIND_SHEAR_EXPONENT);
}

float Guidance::groundGlide(float courseDeg, float h) const {
  // A headwind stronger than the airspeed gives ~0 ground speed, so the
  // height needed becomes huge and the planner will not zig-zag into it.
  return fmaxf(wind_.groundSpeedAlong(courseDeg, windScale(h)), 0.05f) / sinkRate();
}

float Guidance::heightToFly(const Vec2& from, const Vec2& to, float hStart) const {
  Vec2 d = to - from;
  float len = d.length();
  if (len < 0.1f) return 0;
  // use the wind at the middle of the leg's height band
  float first = len / groundGlide(d.bearing(), hStart);
  return len / groundGlide(d.bearing(), hStart - 0.5f * first);
}

// Each corner costs height too: the turn sinks faster and the swing out
// and back onto the next line is wasted distance. Scaled from the learned
// cost of a 180 deg reversal.
float Guidance::cornerLoss(float fromCourse, float toCourse) const {
  return turnLoss_ * fabsf(wrap180(toCourse - fromCourse)) / 180.0f;
}

float Guidance::heightToFinish(const Vec2& from, int idx, float hStart, float arriveCourse) const {
  float used = 0;
  Vec2 prev = from;
  float course = arriveCourse;
  for (int i = idx; i <= lastIdx_; i++) {
    Vec2 to = (i == lastIdx_) ? aimPoint_ : wp_[i];
    float legCourse = (to - prev).bearing();
    if (course >= 0) used += cornerLoss(course, legCourse);
    used += heightToFly(prev, to, hStart - used);
    course = legCourse;
    prev = to;
  }
  return used;
}

// A leg counts as done this far before its end, but never before its first
// half has been flown (short zig-zag legs would otherwise end at once).
float Guidance::acceptDistance(float legLen) { return fminf(WAYPOINT_ACCEPT_M, 0.5f * legLen); }

// Course to fly to stay on the current leg (legFrom_ -> legTo_). The further
// off the line we are, the more directly we head back to it (up to 90 deg).
float Guidance::followLeg(const Vec2& pos, float& along, float& legLen) {
  Vec2 d = legTo_ - legFrom_;
  legLen = d.length();
  if (legLen < 1.0f) {  // degenerate leg: just head for its end point
    along = legLen;
    crossTrack_ = 0;
    return (legTo_ - pos).bearing();
  }
  Vec2 t = d.unit();
  Vec2 r = pos - legFrom_;
  along = r.dot(t);
  crossTrack_ = t.cross(r);  // positive = right of the line
  return wrap360(d.bearing() - atanf(crossTrack_ / PATH_LOOKAHEAD_M) * RAD2DEG);
}

// S-turns either side of the leg to burn spare height that is too small for
// another zig-zag reversal. The weave angle shrinks as the spare height is
// used up, so it settles back onto the line by itself.
float Guidance::weave(float course, float along, float legLen) {
  bool zigzagDone = ZIGZAG_LEG < 0 || toIdx_ > ZIGZAG_LEG + 1;
  bool allowed = WEAVE_ENABLED && zigzagDone && turnDir_ == 0 &&
                 (phase_ == Phase::NAVIGATE || (phase_ == Phase::FINAL && legLen - along > WEAVE_STOP_DIST_M));
  float angle = allowed ? clampf(WEAVE_GAIN_DEG_PER_M * excess_, 0, WEAVE_MAX_DEG) : 0;
  weaveDeg_ = angle > 5.0f ? angle : 0;
  if (weaveDeg_ == 0) return course;
  // start each swing back early by how far the turn will carry us on
  float swingBack = fmaxf(WEAVE_HALF_WIDTH_M - TURN_RADIUS_M * (1.0f - cosf(weaveDeg_ * DEG2RAD)), 2.0f);
  if (crossTrack_ > swingBack) weaveSign_ = -1;  // far enough right: swing left
  if (crossTrack_ < -swingBack) weaveSign_ = +1;
  return wrap360((legTo_ - legFrom_).bearing() + weaveSign_ * weaveDeg_);
}

// Roughly how much height the S-turns can burn on the legs after the
// zig-zag corridor (flown at full weave, at 50% efficiency because the weave
// eases off as the spare height runs out).
float Guidance::weaveCapacity(float h) const {
  if (!WEAVE_ENABLED || ZIGZAG_LEG < 0) return 0;
  float extraPerMetre = 0.5f * (1.0f / cosf(WEAVE_MAX_DEG * DEG2RAD) - 1.0f);
  float cap = 0;
  Vec2 prev = zzB_;
  for (int i = ZIGZAG_LEG + 2; i <= lastIdx_; i++) {
    Vec2 to = wp_[i];
    float len = (to - prev).length();
    if (i == lastIdx_) len = fmaxf(len - WEAVE_STOP_DIST_M, 0.0f);
    cap += len * extraPerMetre / groundGlide((to - prev).bearing(), h);
    prev = to;
  }
  return cap;
}

float Guidance::orbitCourse(const Vec2& pos) const {
  float r = pos.length();
  float dir = ORBIT_CLOCKWISE ? 1.0f : -1.0f;
  return wrap360(pos.bearing() + dir * (90.0f + atanf((r - ORBIT_RADIUS_M) / PATH_LOOKAHEAD_M) * RAD2DEG));
}

// How far back along the zig-zag corridor to fly before turning forward
// again, sized so the extra distance burns exactly the excess height.
// 0 = no reversal, carry on along the path.
float Guidance::planReversal(float height) {
  Vec2 ab = zzB_ - zzA_;
  float len = ab.length();
  // Height needed from B onward. After the zig-zag we will pass B at about
  // that height, so evaluate the wind there (second pass).
  float need = heightToFinish(zzB_, ZIGZAG_LEG + 2, height, ab.bearing()) + ALT_MARGIN_M;
  need = heightToFinish(zzB_, ZIGZAG_LEG + 2, need, ab.bearing()) + ALT_MARGIN_M;
  float fwd = ab.bearing();
  float hMid = 0.5f * (height + need);
  // Too windy to fly the corridor both ways: the parafoil would just be
  // blown downwind of it while trying to reverse.
  float scale = windScale(height);
  if (wind_.groundSpeedAlong(fwd, scale) < ZIGZAG_MIN_GROUND_SPEED_MPS ||
      wind_.groundSpeedAlong(fwd + 180.0f, scale) < ZIGZAG_MIN_GROUND_SPEED_MPS) {
    return 0;
  }
  float heightPerMetre = 1.0f / groundGlide(fwd + 180.0f, hMid) + 1.0f / groundGlide(fwd, hMid);

  // A proper racetrack: the back and forward legs fly off each turn's wind drift
  float turns = reversalCost(fwd + 180.0f, height) + reversalCost(fwd, hMid);
  float spare = height - need - turns;
  float d = spare / heightPerMetre;
  if (d >= 2.0f * TURN_RADIUS_M) {
    d = fminf(d, len);
    plannedSpare_ = spare - d * heightPerMetre;
    return d;
  }

  // Not enough for a proper racetrack. The shortest reversal is a loop:
  // two turns with no real back leg, so the drift of both turns has to be
  // made up on the leg after B. Fly it anyway if arriving a little low
  // beats arriving with more spare height than the S-turns can burn.
  Vec2 next = (ZIGZAG_LEG + 2 == lastIdx_) ? aimPoint_ : wp_[ZIGZAG_LEG + 2];
  float loop = fmaxf(2.0f * turnLoss_ + 2.0f * windDriftCost((next - zzB_).bearing(), hMid), 1.0f);
  float spareLoop = height - need - loop - ZIGZAG_MIN_LEG_M * heightPerMetre;
  float shortIfLoop = fmaxf(0.0f, -(ALT_MARGIN_M + spareLoop));
  float longIfSkip = fmaxf(0.0f, height - need + ALT_MARGIN_M - weaveCapacity(need));
  if (shortIfLoop > longIfSkip) return 0;
  d = clampf(ZIGZAG_MIN_LEG_M + fmaxf(spareLoop, 0.0f) / heightPerMetre, ZIGZAG_MIN_LEG_M, len);
  // what is left over beyond ALT_MARGIN_M if it goes to plan (negative = eats into the margin)
  plannedSpare_ = spareLoop - (d - ZIGZAG_MIN_LEG_M) * heightPerMetre;
  return d;
}

// Height that the wind takes or gives during a reversal turn: while the
// parafoil turns, the wind carries it along. Onto a leg into the wind that
// drift has to be flown back (costly); onto a downwind leg it is free distance.
float Guidance::windDriftCost(float newCourse, float h) const {
  float turnTime = PI_F * TURN_RADIUS_M / AIRSPEED_NOMINAL_MPS;
  float windAlong = (wind_.wind() * windScale(h)).dot(Vec2::fromBearing(newCourse));
  return -windAlong * turnTime / groundGlide(newCourse, h);
}

// Total height a 180 deg reversal onto newCourse costs
float Guidance::reversalCost(float newCourse, float h) const {
  return fmaxf(turnLoss_ + windDriftCost(newCourse, h), 1.0f);
}

void Guidance::startReversal(const NavState& s, int8_t dir) {
  turnDir_ = dir;
  turnStartH_ = s.height;
  turnStartT_ = s.t;
  measuringTurn_ = true;
}

void Guidance::onWaypointReached(const NavState& s) {
  if (phase_ == Phase::ZIGZAG_BACK) {
    Vec2 r = legTo_ - zzOffset_;  // back onto the corridor line
    setPhase(Phase::ZIGZAG_FWD, s.t);
    startLeg(r, zzB_);
    startReversal(s, reversalTurn());
    return;
  }

  // Reached wp_[toIdx_]. At the end of the zig-zag corridor, decide whether
  // we have enough spare height for another back-and-forth.
  // (Not if the wind has pushed us well off the corridor: reversing from
  // there would not stay inside it.)
  Vec2 ab = (zzB_ - zzA_).unit();
  bool onCorridor = fabsf(ab.cross(s.pos - zzA_)) < ZIGZAG_SPACING_M + TURN_RADIUS_M;
  if (ZIGZAG_LEG >= 0 && toIdx_ == ZIGZAG_LEG + 1 && onCorridor) {
    float d = planReversal(s.height);
    if (d > 0) {
      setPhase(Phase::ZIGZAG_BACK, s.t);
      Vec2 b = zzB_ + zzOffset_;
      startLeg(b, b + (zzA_ - zzB_).unit() * d);
      startReversal(s, reversalTurn());
      zigzags_++;
      return;
    }
  }

  Vec2 from = wp_[toIdx_];
  toIdx_++;
  setPhase(toIdx_ == lastIdx_ ? Phase::FINAL : Phase::NAVIGATE, s.t);
  startLeg(from, wp_[toIdx_]);
}

GuidanceCommand Guidance::update(const NavState& s, float dt) {
  GuidanceCommand cmd;
  if (s.gpsNew) lastGpsT_ = s.t;
  float gpsAge = s.t - lastGpsT_;
  bool gpsLost = !s.gpsOk || gpsAge > GPS_TIMEOUT_S;

  // ---- Estimators, only in steady gliding flight ----
  bool gliding = phase_ == Phase::NAVIGATE || phase_ == Phase::ZIGZAG_BACK ||
                 phase_ == Phase::ZIGZAG_FWD || phase_ == Phase::ORBIT;
  if ((gliding || phase_ == Phase::FINAL) && s.gpsNew && s.headingOk && USE_COMPASS_HEADING) {
    float gpsDt = clampf(s.t - lastWindT_, 0.05f, 1.0f);
    wind_.update(s.vel.n, s.vel.e, s.headingDeg, gpsDt);
    windRefH_.update(s.height, gpsDt);  // roughly the height the wind estimate belongs to
  }
  if (s.gpsNew) lastWindT_ = s.t;
  // Straight-line sink only: turns sink faster, and the planner already
  // charges each reversal separately (turnLoss_)
  if (gliding && turnDir_ == 0 && fabsf(s.yawRateDps) < 8.0f) sink_.update(-s.vz, dt);

  // ---- Pre-flight phases ----
  switch (phase_) {
    case Phase::PREFLIGHT:
      if (s.deployReleased || forceDeploy_) {
        setPhase(Phase::DEPLOY, s.t);
      } else if (s.height > ARM_HEIGHT_M) {
        setPhase(Phase::ARMED, s.t);
      }
      forceDeploy_ = false;
      return cmd;

    case Phase::ARMED:
      dropTimer_ = (-s.vz > DROP_SINK_MPS) ? dropTimer_ + dt : 0;
      if (s.deployReleased || forceDeploy_ || dropTimer_ >= DROP_CONFIRM_S) {
        setPhase(Phase::DEPLOY, s.t);
      } else if (s.height < 0.5f * ARM_HEIGHT_M) {
        setPhase(Phase::PREFLIGHT, s.t);  // carried back down without a drop
      }
      forceDeploy_ = false;
      return cmd;

    case Phase::DEPLOY:
      if (timeInPhase(s.t) >= DEPLOY_STABILIZE_S) {
        toIdx_ = 0;
        setPhase(toIdx_ == lastIdx_ ? Phase::FINAL : Phase::NAVIGATE, s.t);
        startLeg(s.pos, wp_[toIdx_]);
      }
      return cmd;

    case Phase::FLARE: {
      cmd.symmetric = FLARE_PULL;
      bool still = fabsf(s.vz) < 0.4f && s.vel.length() < 1.5f;
      landedTimer_ = still ? landedTimer_ + dt : 0;
      if (landedTimer_ > 2.0f || timeInPhase(s.t) > 10.0f) setPhase(Phase::LANDED, s.t);
      return cmd;
    }

    case Phase::LANDED:
      cmd.symmetric = LANDED_PULL;
      return cmd;

    default:
      break;
  }

  // ---- In flight: checks that apply to every phase ----
  if (s.height < FLARE_HEIGHT_M) {
    setPhase(Phase::FLARE, s.t);
    landedTimer_ = 0;
    cmd.symmetric = FLARE_PULL;
    return cmd;
  }

  if (!gpsLost && s.pos.length() > FENCE_RADIUS_M && phase_ != Phase::SPIRAL) {
    if (FENCE_ACTION_SPIRAL) {
      setPhase(Phase::SPIRAL, s.t);
    } else if (phase_ != Phase::FINAL) {
      toIdx_ = lastIdx_;
      setPhase(Phase::FINAL, s.t);
      startLeg(s.pos, wp_[lastIdx_]);
    }
  }

  if (phase_ == Phase::SPIRAL) {
    cmd.mode = GuidanceCommand::DIRECT;
    cmd.steer = ORBIT_CLOCKWISE ? SPIRAL_STEER : -SPIRAL_STEER;
    return cmd;
  }

  if (gpsLost) {
    if (gpsAge > GPS_LOST_SPIRAL_S) {
      setPhase(Phase::SPIRAL, s.t);
    } else if (holdHeadingValid_ && s.headingOk) {
      cmd.mode = GuidanceCommand::HEADING;  // keep flying the last heading we wanted
      cmd.targetDeg = holdHeading_;
    }
    return cmd;
  }

  // ---- Path following ----
  float course = 0;
  float along = 0, legLen = 0;
  switch (phase_) {
    case Phase::NAVIGATE:
    case Phase::ZIGZAG_FWD:
      course = followLeg(s.pos, along, legLen);
      excess_ = s.height - heightToFinish(s.pos, toIdx_, s.height, (legTo_ - legFrom_).bearing());
      // turnDir_ != 0 means a zig-zag reversal turn is still under way
      if (turnDir_ == 0 && along >= legLen - acceptDistance(legLen) &&
          (phase_ != Phase::ZIGZAG_FWD || timeInPhase(s.t) > MIN_ZIGZAG_LEG_S)) {
        onWaypointReached(s);
      }
      break;

    case Phase::ZIGZAG_BACK: {
      course = followLeg(s.pos, along, legLen);
      // height left after: back to the reversal point, the turn, forward to B, then the rest
      float fwd = (zzB_ - zzA_).bearing();
      float h = s.height - heightToFly(s.pos, legTo_, s.height);
      h -= reversalCost(fwd, h);
      h -= heightToFly(legTo_ - zzOffset_, zzB_, h);
      excess_ = h - heightToFinish(zzB_, ZIGZAG_LEG + 2, h, fwd);
      if (turnDir_ != 0 || timeInPhase(s.t) < MIN_ZIGZAG_LEG_S) {
        // still in the reversal turn
      } else if (along >= legLen - acceptDistance(legLen)) {
        onWaypointReached(s);
      } else if (s.gpsNew && excess_ < fminf(0.0f, plannedSpare_) - 0.5f) {
        // Sinking faster than planned (or the wind changed). Either turn
        // forward again right here, or give up the zig-zag and carry on
        // along the path from B, whichever lands closer: turning now leaves
        // us short, giving up leaves spare height the S-turns may not burn.
        float shortIfTurn = fmaxf(0.0f, -(excess_ + heightToFly(s.pos, legTo_, s.height)));
        float abortExcess = s.height - heightToFinish(s.pos, ZIGZAG_LEG + 2, s.height, s.vel.bearing());
        float longIfAbort = fmaxf(0.0f, abortExcess - weaveCapacity(s.height));
        if (shortIfTurn <= longIfAbort) {
          Vec2 u = (zzA_ - zzB_).unit();
          legTo_ = legFrom_ + u * clampf(along, ZIGZAG_MIN_LEG_M, (zzA_ - zzB_).length());
          onWaypointReached(s);
        } else {
          toIdx_ = ZIGZAG_LEG + 2;
          setPhase(toIdx_ == lastIdx_ ? Phase::FINAL : Phase::NAVIGATE, s.t);
          startLeg(zzB_, wp_[toIdx_]);
        }
      }
      break;
    }

    case Phase::FINAL:
      course = followLeg(s.pos, along, legLen);
      excess_ = s.height - heightToFly(s.pos, aimPast(legFrom_, legTo_), s.height);
      // Too high: brake to steepen the glide. Too low: brakes off.
      cmd.symmetric = clampf(FINAL_BRAKE_GAIN * excess_, 0, FINAL_BRAKE_MAX);
      if (along >= legLen && s.height > FLARE_HEIGHT_M + ORBIT_TRIGGER_EXCESS_M) {
        setPhase(Phase::ORBIT, s.t);  // passed over the target still high
      }
      break;

    case Phase::ORBIT:
      course = orbitCourse(s.pos);
      // leave the orbit once a straight glide to the target (after turning
      // toward it from the current course) uses up the remaining height
      excess_ = s.height - heightToFly(s.pos, aimPast(s.pos, wp_[lastIdx_]), s.height) -
                cornerLoss(s.vel.bearing(), (wp_[lastIdx_] - s.pos).bearing());
      if (excess_ <= 0) {
        setPhase(Phase::FINAL, s.t);
        startLeg(s.pos, wp_[lastIdx_]);
      }
      break;

    default:
      break;
  }
  // If a leg or phase just changed, steer for the new one straight away
  // (the forced turn direction below needs to see the new course).
  course = (phase_ == Phase::ORBIT) ? orbitCourse(s.pos) : followLeg(s.pos, along, legLen);
  course = weave(course, along, legLen);

  desiredCourse_ = course;
  if (USE_COMPASS_HEADING && s.headingOk) {
    cmd.mode = GuidanceCommand::HEADING;
    cmd.targetDeg = wind_.headingForCourse(course);
    holdHeading_ = cmd.targetDeg;
    holdHeadingValid_ = true;
  } else {
    cmd.mode = GuidanceCommand::COURSE;
    cmd.targetDeg = course;
  }

  // A forced reversal direction only lasts until the turn is mostly done
  if (turnDir_ != 0) {
    float actual = (USE_COMPASS_HEADING && s.headingOk) ? s.headingDeg : s.vel.bearing();
    if (fabsf(wrap180(cmd.targetDeg - actual)) < 60.0f) turnDir_ = 0;
  }
  // Learn how much height a reversal really costs on this canopy: from the
  // start of the turn until we are back on a line, minus what the distance
  // made good along the new leg would have cost anyway (negative when the
  // wind carried us back behind the leg's start). The wind drift part is
  // modelled separately, so only the calm-air part is learned.
  if (measuringTurn_) {
    bool zigzagging = phase_ == Phase::ZIGZAG_BACK || phase_ == Phase::ZIGZAG_FWD;
    if (!zigzagging || s.t - turnStartT_ > 30.0f) {
      measuringTurn_ = false;
    } else if (turnDir_ == 0 && fabsf(crossTrack_) < 5.0f && s.t - turnStartT_ > 2.0f) {
      measuringTurn_ = false;
      float legCourse = (legTo_ - legFrom_).bearing();
      float made = along / groundGlide(legCourse, s.height);
      float calm = turnStartH_ - s.height - made - windDriftCost(legCourse, s.height);
      turnLoss_ = 0.5f * turnLoss_ + 0.5f * clampf(calm, 1.0f, 40.0f);
    }
  }
  cmd.turnDir = turnDir_;
  return cmd;
}
