# Guided parafoil flight controller

1.5U CubeSat (~1.5 kg) under a Hacker Cloud 1.0 parafoil, ESP32-WROOM, two
sail-winch servos with helical-groove spools on the steering lines.

## What it does

```
PREFLIGHT -> ARMED -> DEPLOY -> NAVIGATE -> ZIGZAG (racetrack) -> NAVIGATE -> FINAL -> FLARE -> LANDED
                                                              (ORBIT if it overshoots high; SPIRAL if lost)
```

1. **Arming and drop detection.** Once carried above `ARM_HEIGHT_M`, a sustained descent (or an optional breakwire) starts the flight.
   The lines stay neutral for `DEPLOY_STABILIZE_S` while the canopy opens.
2. **Path following.** Flies the waypoint path in `Mission.h` leg by leg. A cross-track law steers back onto each line, so it holds the
   path you drew to avoid the trees and buildings, not just a heading toward the next point.
3. **Zig-zag energy management.** The glide ratio is high, so the parafoil always arrives with spare height. On the zig-zag leg it flies
   a racetrack: forward on the corridor line, a 180° turn, back on a parallel line one turn-diameter to the clear side, then the same
   turn again. Each pass is sized in flight from the measured sink rate, wind and turn cost, so it burns just the spare height.
4. **S-turns.** Spare height too small for another zig-zag pass is burned by weaving ±12 m either side of the remaining legs.
5. **Final approach.** Symmetric brakes trim the glide. If it still passes over the target high, it orbits the target (spiral) and comes back.
6. **Flare** at `FLARE_HEIGHT_M`, then the canopy is stalled on the ground so it doesn't drag the CubeSat.

Running in the background:
- **Wind and airspeed estimator.** Least squares on GPS velocity vs compass heading. The zig-zag reversals give it the heading changes it needs.
- **Wind-shear model.** Weaker wind near the ground.
- **Learned turn cost.** The height a reversal really costs on your canopy, measured at every reversal.

The heading loop is a PID (your original structure) with these fixes:
- the integral now uses `ki`, with anti-windup;
- the derivative uses the gyro yaw rate instead of accumulating the error;
- target headings are crabbed into the wind.

### Safety behaviour
| Situation | Action |
|---|---|
| Leaves `FENCE_RADIUS_M` of the target | spiral down in place (or fly straight at the target, `FENCE_ACTION_SPIRAL`) |
| GPS lost | hold the last heading; after `GPS_LOST_SPIRAL_S` spiral down in place |
| Too windy to fly the corridor both ways | skip the zig-zag |
| Pushed far off the corridor | no more reversals |
| Sensors not connected (`SIMULATION 0` with the stubs) | stays in PREFLIGHT, lines neutral |

## Files

| File | What's in it |
|---|---|
| `Config.h` | every tunable number: gains, servo/spool geometry, thresholds. `[MEASURE]` = get it from test flights |
| `Mission.h` | target, waypoints, zig-zag corridor (example coordinates: **replace them**) |
| `Guidance.*` | phase state machine, path following, zig-zag / S-turn planning |
| `FlightCore.*` | sensors → guidance → PID → line mixer (no Arduino code, so the PC sim runs it too) |
| `WindEstimator.*`, `HeadingController.*`, `GeoMath.h` | estimators, PID, maths |
| `Sensors.cpp` | **plug your IMU / GPS / altitude code in here** |
| `Winch.*` | servo pulses: line pull (mm) → spool turns → pulse width |
| `ParafoilSim.*` | simulated parafoil for bench tests |
| `tools/pc_sim.cpp`, `tools/run_pc_sim.ps1` | runs many simulated drops on a PC |
| `tools/plot_sim.py` | draws a flight log (sim or real Serial telemetry) as an SVG |

## Getting started

1. **Mission:** put your target and waypoints in `Mission.h` (Google Maps coordinates, 6+ decimals).
   - Lay the zig-zag corridor across the expected wind if you can.
   - Point the final leg into the wind.
   - On boot the Serial monitor prints the mission in metres, so you can sanity-check it.
2. **Bench test in simulation** (`SIMULATION 1`, the default): flash it, open the Serial monitor at 115200 and send `S`.
   The ESP32 flies a simulated drop and **drives the real servos**, so you can watch the winches work.
   Copy the CSV lines into a file and run `py tools\plot_sim.py file.csv` to see the track.
3. **Hook up sensors** in `Sensors.cpp`, set `SIMULATION 0`. Check the signs listed at the top of that file. Yaw rate sign matters most.
4. **PC simulation** (optional, for trying mission or gain changes): `py -m pip install ziglang`, then
   `powershell -File tools\run_pc_sim.ps1`.

## Test plan before a real drop

1. **Winch direction and travel:** in sim mode, check that a right turn (`steer` > 0) reels in the *right* line.
   Flip `WINCH_*_REVERSED` if not. Check full pull equals `LINE_PULL_MAX_MM`.
2. **IMU check:** turning the board clockwise must increase `yawDeg` and give a positive `yawRateDps`. Calibrate the compass
   *with the servos powered and moving*.
3. **Walk test:** walk the path with the board and watch `n`, `e`, `xtrk` in the telemetry.
4. **First drops with `ZIGZAG_LEG = -1` and `WEAVE_ENABLED = false`.** Straight path and final only.
   From the log, read the estimated `va` (airspeed) and `sink`, and the turn radius. Put them in `Config.h`.
5. Then enable the zig-zag and S-turns.

## Simulation results (current code)

10 drops per scenario, example mission, release 150 m up and ~320 m out unless noted. The simulator deliberately differs from the
controller's assumptions: airspeed −6%, a canopy turn bias, 4° compass error, wind shear, gusts, GPS/baro noise, and slow winches.

| Scenario | Mean miss | Worst |
|---|---|---|
| Calm | 12.6 m | 28 m |
| 2 m/s from N (tailwind on final) | 12.6 m | 34 m |
| 3 m/s from N | 6.5 m | 11 m |
| 2.5 m/s from W (crosswind) | 19.2 m | 46 m |
| High release 220 m | 5.9 m | 8 m |
| Released near the corridor | 16.1 m | 44 m |
| 4 m/s from NW | 8.1 m | 40 m |
| 2 m/s from S / 2.5 m/s from E / low 80 m release | 28–122 m | **not enough height**: released downwind or too low, lands short on the path |

These numbers come from a simplified model of the canopy. Real accuracy depends on the measured values in `Config.h`.

## Known limitations

- **Release point:** it must be upwind of the path and high enough. The controller can only burn height, not gain it.
  If it starts short, it lands short *on the path*. It never shortcuts, because a shortcut could cross the obstacles you routed around.
- **Barometer drift** over a long flight moves the flare point. A downward rangefinder (e.g. a small lidar) would make the flare much better.
- **Simulator:** it is a kinematic model, not a dynamic one. It has no pendulum swing, no stall, and no turbulence.
