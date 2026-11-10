/*
 * Guided parafoil flight controller - 1.5U CubeSat, Hacker Cloud 1.0, ESP32-WROOM
 *
 * Inputs: IMU heading (from the filtered quaternion), GPS position/velocity
 * (Kalman filtered) and barometric altitude. Outputs: two winch servos that
 * pull the left and right steering lines.
 *
 * Every loop (50 Hz):
 *   1. Guidance follows the waypoint path in Mission.h. Leg by leg it works out
 *      the course that brings the parafoil back onto the line.
 *   2. Because the glide ratio is high, the parafoil arrives with height to
 *      spare. On the zig-zag leg it flies back and forth, and each pass is
 *      sized from the measured sink rate and wind to burn just the spare height.
 *   3. On final approach symmetric brakes trim the glide onto the target.
 *      If it still overshoots high, it orbits (spirals) over the target.
 *   4. A heading PID turns the desired heading into a differential line pull.
 *      It is crabbed into the wind using the wind estimate.
 *   5. Near the ground both lines are pulled to flare.
 *
 * Files:
 *   Config.h        all tunable numbers (gains, servo/spool geometry, thresholds)
 *   Mission.h       target, waypoints, zig-zag corridor
 *   Guidance.*      phase state machine, path following, zig-zag planning
 *   FlightCore.*    sensors -> guidance -> PID -> line mixer
 *   Sensors.cpp     plug your IMU / GPS / altitude code in here
 *   Winch.*         servo pulse output
 *   ParafoilSim.*   simulated parafoil for bench testing (SIMULATION 1)
 *
 * Serial (115200): CSV telemetry at 5 Hz. Commands:
 *   S  start a simulated drop (SIMULATION 1)
 *   R  reset the controller
 *   D  force deploy (skip the drop detector) - bench testing only
 *   M  print the mission
 */

#include "Config.h"
#include "FlightCore.h"
#include "Mission.h"
#include "Sensors.h"
#include "Winch.h"
#if SIMULATION
#include "ParafoilSim.h"
#endif

FlightCore core;
Winch winchLeft(PIN_WINCH_LEFT, WINCH_LEFT_REVERSED);
Winch winchRight(PIN_WINCH_RIGHT, WINCH_RIGHT_REVERSED);
SensorData sensors;

#if SIMULATION
ParafoilSim sim;
bool simRunning = false;
bool simReported = false;
#endif

void printMission() {
  const LocalFrame& f = core.frame();
  const Guidance& g = core.guidance();
  Serial.println(F("# ---- Mission (metres from target, N/E) ----"));
  for (int i = 0; i < PATH_COUNT; i++) {
    Vec2 p = f.toLocal(PATH[i].lat, PATH[i].lon);
    Serial.printf("# PATH[%d]  N %7.1f  E %7.1f%s\n", i, p.n, p.e,
                  i == ZIGZAG_LEG ? "  <- zig-zag corridor start" : (i == ZIGZAG_LEG + 1 ? "  <- zig-zag corridor end" : ""));
  }
  Vec2 last = f.toLocal(PATH[PATH_COUNT - 1].lat, PATH[PATH_COUNT - 1].lon);
  Serial.printf("# TARGET   N %7.1f  E %7.1f   final approach %.0f m, course %.0f deg\n", 0.0, 0.0,
                last.length(), (Vec2(0, 0) - last).bearing());
  Vec2 first = f.toLocal(PATH[0].lat, PATH[0].lon);
  Serial.printf("# Height needed from PATH[0] with nominal glide %.1f (no zig-zag): %.1f m\n",
                AIRSPEED_NOMINAL_MPS / SINK_RATE_NOMINAL_MPS, g.heightToFinish(first, 1, 100.0f));
}

void printHeader() {
  Serial.println(F("t,phase,n,e,h,vz,gs,hdg,tgt,xtrk,excess,wind_n,wind_e,va,sink,steer,left,right,wp,zz"));
}

void printTelemetry() {
  const NavState& n = core.nav();
  const Guidance& g = core.guidance();
  Vec2 w = g.wind().wind();
  Serial.printf("%.2f,%s,%.1f,%.1f,%.1f,%.2f,%.1f,%.0f,%.0f,%.1f,%.1f,%.1f,%.1f,%.2f,%.2f,%.2f,%.2f,%.2f,%d,%d\n",
                n.t, phaseName(g.phase()), n.pos.n, n.pos.e, n.height, n.vz, n.vel.length(), n.headingDeg,
                core.command().targetDeg, g.crossTrack(), g.excessHeight(), w.n, w.e, g.wind().airspeed(),
                g.sinkRate(), core.steer(), core.winch().left, core.winch().right, g.targetWaypoint(),
                g.zigzagCount());
}

void handleSerial() {
  while (Serial.available()) {
    char c = toupper(Serial.read());
    if (c == 'R') {
      core.begin();
#if SIMULATION
      simRunning = false;
#endif
      Serial.println(F("# reset"));
    } else if (c == 'D') {
      core.forceDeploy();
      Serial.println(F("# forced deploy"));
    } else if (c == 'M') {
      printMission();
#if SIMULATION
    } else if (c == 'S') {
      core.begin();
      SimParams p;
      p.seed = esp_random();
      sim.reset(p, core.frame());
      simRunning = true;
      simReported = false;
      Serial.println(F("# simulated drop started"));
      printHeader();
#endif
    }
  }
}

void setup() {
  Serial.begin(115200);
  winchLeft.begin();  // lines neutral before anything else
  winchRight.begin();
  sensorsBegin();
  core.begin();
  delay(500);
  Serial.println(F("# Parafoil flight controller"));
#if SIMULATION
  Serial.println(F("# SIMULATION build: send S to start a simulated drop"));
#endif
  printMission();
  printHeader();
}

void loop() {
  // Fixed 50 Hz rate. vTaskDelayUntil keeps the period steady and lets the
  // ESP32's background tasks run (replaces the std::chrono sleep).
  static TickType_t lastWake = xTaskGetTickCount();
  static uint32_t lastUs = micros();
  static uint32_t lastTelemetryUs = 0;
  vTaskDelayUntil(&lastWake, pdMS_TO_TICKS((uint32_t)(1000.0f / LOOP_HZ)));

  uint32_t nowUs = micros();
  float dt = clampf((nowUs - lastUs) * 1e-6f, 0.5f * DT, 3.0f * DT);
  lastUs = nowUs;

  handleSerial();

#if SIMULATION
  if (!simRunning) {
    winchLeft.setPull(0);
    winchRight.setPull(0);
    return;
  }
  sim.fillSensors(sensors);
#else
  sensorsRead(sensors);
#endif

  WinchCommand w = core.step(sensors, dt);
  winchLeft.setPull(w.left);
  winchRight.setPull(w.right);

#if SIMULATION
  sim.step(w, dt);
  if (sim.onGround() && !simReported) {
    simReported = true;
    Serial.printf("# TOUCHDOWN %.1f m from target (N %.1f, E %.1f), %.1f m/s, %d zig-zags, %.0f s\n",
                  sim.pos().length(), sim.pos().n, sim.pos().e, sim.touchdownSpeed(),
                  core.guidance().zigzagCount(), sim.t());
  }
  if (simReported && core.guidance().phase() == Phase::LANDED) simRunning = false;
#endif

  if (nowUs - lastTelemetryUs >= (uint32_t)(1e6f / TELEMETRY_HZ)) {
    lastTelemetryUs = nowUs;
    printTelemetry();
  }
}
