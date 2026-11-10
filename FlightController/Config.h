#pragma once
/*
 * Config.h - every tunable number for the flight controller lives here.
 *
 * Values marked [MEASURE] are placeholders. Get them from test drops
 * (the telemetry log prints the estimated airspeed, sink rate and wind).
 */

// ---------------------------------------------------------------------------
// Build mode
// ---------------------------------------------------------------------------
// 1 = the ESP32 flies a simulated parafoil instead of reading the real sensors.
//     The servos still move, so you can bench-test the whole system.
//     Send 'S' over Serial to start a simulated drop, 'R' to reset.
// 0 = real flight. Sensors.cpp must be wired to your IMU / GPS / altitude code.
#ifndef SIMULATION
#define SIMULATION 1
#endif

// ---------------------------------------------------------------------------
// Loop timing
// ---------------------------------------------------------------------------
constexpr float LOOP_HZ = 50.0f;
constexpr float DT = 1.0f / LOOP_HZ;
constexpr float TELEMETRY_HZ = 5.0f;

// ---------------------------------------------------------------------------
// Parafoil performance (Hacker Cloud 1.0 + 1.5 kg payload)
// ---------------------------------------------------------------------------
constexpr float AIRSPEED_NOMINAL_MPS = 7.0f;   // [MEASURE] trim airspeed
constexpr float SINK_RATE_NOMINAL_MPS = 1.4f;  // [MEASURE] trim sink rate
constexpr float TURN_RADIUS_M = 12.0f;         // [MEASURE] radius at ~full steering
constexpr float TURN_180_ALT_LOSS_M = 8.0f;    // [MEASURE] total height lost in one 180 deg reversal
                                               // (starting guess: re-learned in flight at every reversal)

// ---------------------------------------------------------------------------
// Heading source
// ---------------------------------------------------------------------------
// true  = steer on the IMU heading (fast, lets us estimate wind; the compass
//         must be calibrated and kept away from the servo motors and wires)
// false = steer on the GPS course over ground (no compass needed, but slower
//         and no wind estimate)
constexpr bool USE_COMPASS_HEADING = true;
constexpr float MAG_DECLINATION_DEG = 0.0f;    // east positive, look it up for your site

// ---------------------------------------------------------------------------
// Heading PID (error in degrees -> steering command -1..+1, +1 = full right)
// ---------------------------------------------------------------------------
constexpr float HEADING_KP = 0.012f;      // full steering at ~80 deg error
constexpr float HEADING_KI = 0.0015f;     // trims out a canopy that turns on its own
constexpr float HEADING_KD = 0.004f;      // per deg/s of yaw rate (damping)
constexpr float HEADING_I_LIMIT = 0.25f;  // max steering the integral may hold
constexpr float HEADING_I_ZONE_DEG = 30.0f; // integrate only when the error is small

// ---------------------------------------------------------------------------
// Path following
// ---------------------------------------------------------------------------
constexpr float PATH_LOOKAHEAD_M = 18.0f;  // bigger = smoother, smaller = tighter on the line
constexpr float WAYPOINT_ACCEPT_M = 8.0f;  // switch to the next leg this far before a waypoint

// ---------------------------------------------------------------------------
// Zig-zag energy management
// ---------------------------------------------------------------------------
constexpr float ALT_MARGIN_M = 4.0f;       // extra height kept in hand when deciding to zig-zag
constexpr float ZIGZAG_MIN_LEG_M = 5.0f;   // shortest back leg (shorter ones fly as a loop)
constexpr float ZIGZAG_MIN_GROUND_SPEED_MPS = 2.0f; // no zig-zag if either direction is slower than this over the ground
// Wind drops off near the ground. The planner scales the measured wind by
// (height / height it was measured at) ^ exponent. 0.14 over open ground,
// 0.2-0.3 around trees and buildings, 0 to turn it off.
constexpr float WIND_SHEAR_EXPONENT = 0.14f;
// The zig-zag is flown as a racetrack: forward along the corridor line, a
// 180 deg turn, back along a parallel line ZIGZAG_SPACING_M away, and the
// same turn again onto the corridor line. ZIGZAG_SIDE_RIGHT picks which side
// the back line is on, looking from PATH[ZIGZAG_LEG] to PATH[ZIGZAG_LEG + 1].
// Pick the side with clear space. The corridor needs about
// ZIGZAG_SPACING_M + TURN_RADIUS_M of clear width on that side.
constexpr bool  ZIGZAG_SIDE_RIGHT = true;
constexpr float ZIGZAG_SPACING_M = 2.0f * TURN_RADIUS_M;  // = one turn diameter, so no S-turn is needed

// ---------------------------------------------------------------------------
// S-turn weaving: burns the last bit of spare height (less than one zig-zag
// reversal is worth) on the legs after the zig-zag and on final approach
// ---------------------------------------------------------------------------
constexpr bool  WEAVE_ENABLED = true;
constexpr float WEAVE_HALF_WIDTH_M = 12.0f;   // swing at most this far either side of the line
constexpr float WEAVE_GAIN_DEG_PER_M = 12.0f; // weave angle per metre of spare height
constexpr float WEAVE_MAX_DEG = 55.0f;
constexpr float WEAVE_STOP_DIST_M = 30.0f;    // fly the last part of final straight

// ---------------------------------------------------------------------------
// Final approach
// ---------------------------------------------------------------------------
constexpr float FINAL_BRAKE_GAIN = 0.10f;  // symmetric brake per metre of excess height
constexpr float FINAL_BRAKE_MAX = 0.45f;   // [CHECK] never brake more than this on final (stall margin)
constexpr float FLARE_HEIGHT_M = 2.5f;     // [MEASURE] start flare at this height above target
constexpr float FLARE_AIM_PAST_M = 5.0f;   // [MEASURE] the flare steepens the last bit of the glide,
                                           // so plan the glide to a point this far past the target
constexpr float FLARE_PULL = 1.0f;         // both lines, fraction of full pull
constexpr float LANDED_PULL = 1.0f;        // after touchdown: stall the canopy so it does not drag

// Overshoot: if we pass over the target still high, orbit it (spiral down)
constexpr float ORBIT_RADIUS_M = 15.0f;
constexpr bool  ORBIT_CLOCKWISE = true;    // also the direction of an emergency spiral
constexpr float ORBIT_TRIGGER_EXCESS_M = 6.0f; // height above flare height when passing the target that starts an orbit

// ---------------------------------------------------------------------------
// Launch / deploy detection
// ---------------------------------------------------------------------------
constexpr float ARM_HEIGHT_M = 20.0f;      // must be carried this high before a drop is accepted
constexpr float DROP_SINK_MPS = 3.0f;      // descent rate that means "we have been dropped"
constexpr float DROP_CONFIRM_S = 0.3f;
constexpr float DEPLOY_STABILIZE_S = 3.0f; // lines neutral while the canopy opens and stops swinging
constexpr int   PIN_DEPLOY_SWITCH = -1;    // optional breakwire to GND, reads HIGH once broken on release. -1 = unused

// ---------------------------------------------------------------------------
// Safety
// ---------------------------------------------------------------------------
constexpr float FENCE_RADIUS_M = 400.0f;   // distance from target that counts as "lost"
constexpr bool  FENCE_ACTION_SPIRAL = true; // true = spiral down where we are, false = fly straight at the target
constexpr float SPIRAL_STEER = 0.5f;       // steering used for an emergency spiral
constexpr float GPS_TIMEOUT_S = 1.5f;      // no fix for this long = GPS lost
constexpr float GPS_LOST_SPIRAL_S = 10.0f; // GPS lost this long = spiral down in place
constexpr float GPS_MIN_SPEED_FOR_COURSE = 1.5f; // GPS course is noise below this ground speed

// ---------------------------------------------------------------------------
// Winches (sail-winch servos driving helical-groove spools)
// ---------------------------------------------------------------------------
constexpr int   PIN_WINCH_LEFT = 25;
constexpr int   PIN_WINCH_RIGHT = 26;
constexpr bool  WINCH_LEFT_REVERSED = false;  // flip if pulling makes the spool pay out
constexpr bool  WINCH_RIGHT_REVERSED = true;  // mirrored mounting usually needs one reversed
constexpr int   SERVO_PWM_HZ = 50;
constexpr int   SERVO_US_MIN = 1000;          // pulse at the "lines released" end of travel
constexpr int   SERVO_US_MAX = 2000;          // pulse at the other end of travel
constexpr float SERVO_TURNS_FULL_RANGE = 3.5f; // [CHECK] spool turns from US_MIN to US_MAX
constexpr float SPOOL_DIAMETER_MM = 20.0f;    // [MEASURE] groove root diameter + one line diameter
constexpr float LINE_PULL_MAX_MM = 150.0f;    // [MEASURE] line pulled at full brake / full turn
constexpr float STEER_PULL = 0.6f;            // differential pull at steering command 1.0 (fraction of max)
constexpr float TRIM_PULL = 0.0f;             // base pull in normal flight (your old base_winch)

constexpr float PI_F = 3.14159265f;
static_assert(LINE_PULL_MAX_MM <= PI_F * SPOOL_DIAMETER_MM * SERVO_TURNS_FULL_RANGE,
              "LINE_PULL_MAX_MM needs more spool turns than the servo has");
