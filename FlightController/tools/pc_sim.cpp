/*
 * PC simulator: runs the real guidance/control code (FlightCore) against
 * ParafoilSim for many drops with different winds and release points, and
 * prints how close each one lands. Arduino does not compile this folder.
 *
 * Build and run from the sketch folder (any C++17 compiler):
 *   g++ -O2 -std=c++17 -I. tools/pc_sim.cpp Guidance.cpp WindEstimator.cpp HeadingController.cpp FlightCore.cpp ParafoilSim.cpp -o pc_sim
 *   ./pc_sim                 all scenarios, summary table
 *   ./pc_sim 3 run3.csv      scenario 3 only, full 10 Hz log to run3.csv (plot with tools/plot_sim.py)
 *   ./pc_sim 3 run3.csv 42   same with random seed 42 (the summary uses seeds 7919, 15838, ...)
 *   ./pc_sim -v              summary plus the miss of every single drop
 */
#include <stdio.h>
#include <stdlib.h>
#include "Config.h"
#include "FlightCore.h"
#include "ParafoilSim.h"

struct Scenario {
  const char* name;
  Vec2 wind10m;
  Vec2 release;
  float height;
};

static const Scenario SCENARIOS[] = {
    {"calm", {0, 0}, {300, -100}, 150},
    {"2 m/s from N (tailwind on final)", {-2, 0}, {300, -100}, 150},
    {"3 m/s from N", {-3, 0}, {300, -100}, 150},
    {"2 m/s from S (headwind on final)", {2, 0}, {300, -100}, 150},
    {"2.5 m/s from W (crosswind)", {0, 2.5f}, {300, -100}, 150},
    {"2.5 m/s from E (crosswind)", {0, -2.5f}, {300, -100}, 150},
    {"low release 80 m", {-2, 0}, {250, -50}, 80},
    {"high release 220 m", {-2, 0.5f}, {350, -150}, 220},
    {"release near corridor", {-1.5f, 1}, {180, 0}, 120},
    {"4 m/s from NW", {-2.8f, 2.8f}, {300, -100}, 160},
};
static const int N_SCEN = sizeof(SCENARIOS) / sizeof(SCENARIOS[0]);

struct Result {
  float miss, missN, missE, tdSpeed, flightTime;
  int zigzags;
  bool landed;
  Phase phaseAtTouchdown;
};

static Result runOne(const Scenario& sc, uint32_t seed, FILE* csv) {
  FlightCore core;
  core.begin();
  ParafoilSim sim;
  SimParams p;
  p.wind10m = sc.wind10m;
  p.releasePos = sc.release;
  p.releaseHeight = sc.height;
  p.seed = seed;
  sim.reset(p, core.frame());

  SensorData s;
  Result r{};
  Phase tdPhase = Phase::PREFLIGHT;
  bool touched = false;
  float nextLog = 0;
  Phase lastPhase = Phase::PREFLIGHT;
  if (csv) fprintf(csv, "t,phase,n,e,h,true_n,true_e,true_h,hdg,tgt,xtrk,excess,wind_n,wind_e,va,sink,turn_loss,steer,left,right,zz\n");

  for (int i = 0; i < (int)(900 * LOOP_HZ) && !touched; i++) {
    sim.fillSensors(s);
    WinchCommand w = core.step(s, DT);
    sim.step(w, DT);
    const Guidance& g = core.guidance();
    if (g.phase() != lastPhase) {
      if (csv) {
        fprintf(stderr, "%7.1f s  %-11s -> %-11s  h=%.1f  pos=(%.0f,%.0f)\n", sim.t(), phaseName(lastPhase),
                phaseName(g.phase()), sim.height(), sim.pos().n, sim.pos().e);
      }
      lastPhase = g.phase();
    }
    if (csv && sim.t() >= nextLog) {
      nextLog += 0.1f;
      const NavState& n = core.nav();
      Vec2 wv = g.wind().wind();
      fprintf(csv, "%.2f,%s,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.1f,%.1f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.3f,%.3f,%.3f,%d\n",
              sim.t(), phaseName(g.phase()), n.pos.n, n.pos.e, n.height, sim.pos().n, sim.pos().e, sim.height(),
              n.headingDeg, core.command().targetDeg, g.crossTrack(), g.excessHeight(), wv.n, wv.e,
              g.wind().airspeed(), g.sinkRate(), g.turnLoss(), core.steer(), w.left, w.right, g.zigzagCount());
    }
    if (sim.onGround()) {
      touched = true;
      tdPhase = g.phase();
    }
  }
  r.landed = touched;
  r.missN = sim.pos().n;
  r.missE = sim.pos().e;
  r.miss = sim.pos().length();
  r.tdSpeed = sim.touchdownSpeed();
  r.flightTime = sim.t() - p.holdTime;
  r.zigzags = core.guidance().zigzagCount();
  r.phaseAtTouchdown = tdPhase;
  return r;
}

int main(int argc, char** argv) {
  bool verbose = argc >= 2 && argv[1][0] == '-';
  if (argc >= 2 && !verbose) {
    int k = atoi(argv[1]);
    if (k < 0 || k >= N_SCEN) {
      fprintf(stderr, "scenario 0..%d\n", N_SCEN - 1);
      return 1;
    }
    FILE* csv = fopen(argc >= 3 ? argv[2] : "sim_run.csv", "w");
    uint32_t seed = argc >= 4 ? (uint32_t)atoi(argv[3]) : 1;
    Result r = runOne(SCENARIOS[k], seed, csv);
    fclose(csv);
    printf("%s: miss %.1f m (N %.1f, E %.1f), touchdown %.1f m/s in %s, %d zig-zags, %.0f s\n", SCENARIOS[k].name,
           r.miss, r.missN, r.missE, r.tdSpeed, phaseName(r.phaseAtTouchdown), r.zigzags, r.flightTime);
    return 0;
  }

  const int SEEDS = 10;
  printf("%-36s %8s %8s %8s %6s %7s  %s\n", "scenario", "mean_m", "worst_m", "td_m/s", "zz", "time_s", "phase at touchdown");
  float allWorst = 0, allSum = 0;
  for (int k = 0; k < N_SCEN; k++) {
    float sum = 0, worst = 0, td = 0, tt = 0;
    int zz = 0;
    const char* ph = "";
    bool allLanded = true;
    for (int seed = 1; seed <= SEEDS; seed++) {
      Result r = runOne(SCENARIOS[k], seed * 7919, nullptr);
      if (verbose) printf("    seed %5d: miss %5.1f m (N %6.1f, E %6.1f) %d zz\n", seed * 7919, r.miss, r.missN, r.missE, r.zigzags);
      sum += r.miss;
      worst = r.miss > worst ? r.miss : worst;
      td += r.tdSpeed;
      tt += r.flightTime;
      zz += r.zigzags;
      ph = phaseName(r.phaseAtTouchdown);
      allLanded &= r.landed;
    }
    printf("%-36s %8.1f %8.1f %8.1f %6.1f %7.0f  %s%s\n", SCENARIOS[k].name, sum / SEEDS, worst, td / SEEDS,
           (float)zz / SEEDS, tt / SEEDS, ph, allLanded ? "" : "  (DID NOT LAND)");
    allSum += sum / SEEDS;
    allWorst = worst > allWorst ? worst : allWorst;
  }
  printf("\naverage miss %.1f m, worst %.1f m\n", allSum / N_SCEN, allWorst);
  return 0;
}
