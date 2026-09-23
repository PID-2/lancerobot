// harness.cpp — offline test of the solver against real contest maze files.
//
// Simulates a mouse with perfect sensors: at every cell centre it reads the
// true left/front/right walls, asks the solver for a move, refuses to drive
// through a real wall (that is a crash), and repeats until the fast run
// reaches the centre. Prints one line per maze and a summary.
//
// Build:  g++ -std=c++11 -O2 -Wall -I../solver harness.cpp -o harness
// Run:    ./harness ../mazefiles/classic/*.txt
//         ./harness -v ../mazefiles/classic/uk2016f.txt   (verbose trace)

#include "solver.h"
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include <fstream>

using namespace mm;

struct Truth {
  uint8_t walls[CELLS];
  uint8_t start;
  std::vector<uint8_t> goals;
  bool hasWall(uint8_t c, Dir d) const { return (walls[c] >> d) & 1; }
};

// Parse the micromouseonline/mazefiles text format (16x16 classic only).
static bool parseMaze(const char* path, Truth& t, std::string& why) {
  std::ifstream f(path);
  if (!f) { why = "cannot open"; return false; }
  std::vector<std::string> lines; std::string s;
  while (std::getline(f, s)) { while (!s.empty() && (s.back() == '\r' || s.back() == ' ')) s.pop_back(); if (!s.empty()) lines.push_back(s); }
  if (lines.size() != 2 * SIZE + 1) { why = "not 16x16 (" + std::to_string(lines.size()) + " lines)"; return false; }
  for (auto& l : lines) if (l.size() < 4 * SIZE + 1) { why = "short line"; return false; }
  memset(t.walls, 0, sizeof t.walls); t.goals.clear(); t.start = 255;
  for (uint8_t y = 0; y < SIZE; ++y) {
    size_t rowTop = 2 * (SIZE - 1 - y), rowMid = rowTop + 1, rowBot = rowTop + 2;
    for (uint8_t x = 0; x < SIZE; ++x) {
      uint8_t c = cellIndex(x, y);
      size_t col = 4 * x;
      if (lines[rowTop].compare(col + 1, 3, "---") == 0) t.walls[c] |= 1 << N;
      if (lines[rowBot].compare(col + 1, 3, "---") == 0) t.walls[c] |= 1 << S;
      if (lines[rowMid][col] == '|')     t.walls[c] |= 1 << W;
      if (lines[rowMid][col + 4] == '|') t.walls[c] |= 1 << E;
      char mark = lines[rowMid][col + 2];
      if (mark == 'G') t.goals.push_back(c);
      if (mark == 'S') t.start = c;
    }
  }
  if (t.start == 255) t.start = cellIndex(0, 0);
  return true;
}

static bool isCentreGoal(const Truth& t) {
  if (t.goals.size() != 4) return false;
  bool seen[4] = { false, false, false, false };
  for (uint8_t g : t.goals) {
    if (g == cellIndex(7, 7)) seen[0] = true; else if (g == cellIndex(8, 7)) seen[1] = true;
    else if (g == cellIndex(7, 8)) seen[2] = true; else if (g == cellIndex(8, 8)) seen[3] = true;
  }
  return seen[0] && seen[1] && seen[2] && seen[3];
}

// Cost of a path under the planner's weights, starting facing north.
static uint32_t pathCost(const Planner& p, const Path& path) {
  uint32_t cost = 0;
  for (uint16_t i = 0; i < path.length; ++i) {
    cost += p.STRAIGHT;
    if (path.moves[i] == MOVE_LEFT || path.moves[i] == MOVE_RIGHT) cost += p.TURN90;
    else if (path.moves[i] == MOVE_BACK) cost += p.TURN180;
  }
  return cost;
}

struct Result {
  bool ok = false; std::string why;
  int runs = 0; int searchSteps = 0; int fastLen = 0; uint32_t fastCost = 0; uint32_t optCost = 0; int loops = 0;
};

static Result simulate(const Truth& t, bool verbose) {
  Result r;
  Mouse mouse; mouse.begin();
  const int MAX_STEPS = 4000;
  int totalSteps = 0;

  for (int run = 1; run <= 8; ++run) {
    mouse.startRun();
    r.runs = run;
    bool fastRun = (mouse.phase == PHASE_SPEED_RUN);
    int steps = 0;
    if (verbose) printf("  run %d: %s\n", run, fastRun ? "SPEED" : "search");
    for (;;) {
      uint8_t c = mouse.cell; Dir h = mouse.heading;
      mouse.observe(t.hasWall(c, leftOf(h)), t.hasWall(c, h), t.hasWall(c, rightOf(h)));
      Move m = mouse.next();
      if (m == MOVE_STOP) break;
      if (!mouse.moveIsSafe(m)) { r.why = "solver proposed a move through an unknown side"; return r; }
      Dir d = headingAfter(h, m);
      if (t.hasWall(c, d)) { r.why = "CRASH into real wall at (" + std::to_string(cellX(c)) + "," + std::to_string(cellY(c)) + ") " + dirChar(d); return r; }
      if (verbose) printf("    (%2d,%2d) %c -> %c\n", cellX(c), cellY(c), dirChar(h), moveChar(m));
      mouse.applyMove(m);
      if (++steps > MAX_STEPS) { r.why = "step limit exceeded"; return r; }
    }
    totalSteps += steps;
    if (fastRun) {
      if (!mouse.maze.isGoal(mouse.cell)) { r.why = "speed run ended off-goal"; return r; }
      r.fastLen = mouse.fastPath.length;
      r.fastCost = pathCost(mouse.planner, mouse.fastPath);
      r.loops = mouse.exploreLoops;
      r.ok = true;
      break;
    } else {
      r.searchSteps = totalSteps;
    }
  }
  if (!r.ok) { r.why = "never reached a speed run in 8 runs"; return r; }

  // Optimal cost with the whole maze known, same weights.
  Maze full; full.reset();
  for (uint16_t c = 0; c < CELLS; ++c) for (uint8_t d = 0; d < 4; ++d) full.setWall((uint8_t)c, (Dir)d, t.hasWall((uint8_t)c, (Dir)d));
  full.setGoalCentre();
  Path opt; Planner p;
  if (!p.plan(full, cellIndex(0, 0), N, opt)) { r.ok = false; r.why = "maze has no route to centre"; return r; }
  r.optCost = pathCost(p, opt);

  // Second speed run from the same map must also work (the run ladder).
  mouse.startRun();
  if (mouse.phase != PHASE_SPEED_RUN) { r.ok = false; r.why = "second run did not start as speed run"; return r; }
  for (int steps = 0; ; ++steps) {
    uint8_t c = mouse.cell; Dir h = mouse.heading;
    mouse.observe(t.hasWall(c, leftOf(h)), t.hasWall(c, h), t.hasWall(c, rightOf(h)));
    Move m = mouse.next(); if (m == MOVE_STOP) break;
    if (t.hasWall(c, headingAfter(h, m))) { r.ok = false; r.why = "CRASH on second speed run"; return r; }
    mouse.applyMove(m);
    if (steps > MAX_STEPS) { r.ok = false; r.why = "second speed run overran"; return r; }
  }
  if (!mouse.maze.isGoal(mouse.cell)) { r.ok = false; r.why = "second speed run ended off-goal"; }
  return r;
}

int main(int argc, char** argv) {
  bool verbose = false; std::vector<const char*> files;
  for (int i = 1; i < argc; ++i) { if (!strcmp(argv[i], "-v")) verbose = true; else files.push_back(argv[i]); }
  if (files.empty()) { fprintf(stderr, "usage: harness [-v] maze.txt ...\n"); return 2; }

  int tested = 0, passed = 0, scored = 0, skipped = 0, optimal = 0, notStart00 = 0;
  long sumSearch = 0, sumRuns = 0; double sumRatio = 0; double worstRatio = 1;
  std::string worstName;
  for (const char* f : files) {
    Truth t; std::string why;
    if (!parseMaze(f, t, why)) { skipped++; if (verbose) printf("%-40s SKIP %s\n", f, why.c_str()); continue; }
    if (t.start != cellIndex(0, 0)) { skipped++; notStart00++; if (verbose) printf("%-40s SKIP start not (0,0)\n", f); continue; }
    if (!isCentreGoal(t)) { skipped++; if (verbose) printf("%-40s SKIP goal not centre 2x2\n", f); continue; }
    tested++;
    if (verbose) printf("%s\n", f);
    Result r = simulate(t, verbose);
    if (!r.ok && r.why.rfind("never reached", 0) == 0) {
      // Is the centre actually reachable? If not, stopping without a crash is the right answer.
      Maze full; full.reset();
      for (uint16_t c = 0; c < CELLS; ++c) for (uint8_t d = 0; d < 4; ++d) full.setWall((uint8_t)c, (Dir)d, t.hasWall((uint8_t)c, (Dir)d));
      full.setGoalCentre(); full.flood(false);
      if (full.dist[cellIndex(0, 0)] == INF) { passed++; printf("%-40s ok  UNSOLVABLE maze (centre sealed) - solver stopped without crashing\n", f); continue; }
    }
    if (!r.ok) { printf("%-40s FAIL %s\n", f, r.why.c_str()); continue; }
    passed++; scored++;
    double ratio = (double)r.fastCost / r.optCost;
    sumRatio += ratio; sumSearch += r.searchSteps; sumRuns += r.runs;
    if (r.fastCost == r.optCost) optimal++;
    if (ratio > worstRatio) { worstRatio = ratio; worstName = f; }
    printf("%-40s ok  runs=%d loops=%d searchCells=%4d fast=%3d moves cost=%4u opt=%4u %s\n",
           f, r.runs, r.loops, r.searchSteps, r.fastLen, r.fastCost, r.optCost, r.fastCost == r.optCost ? "OPTIMAL" : "");
  }
  printf("\n=== %d tested, %d passed, %d failed, %d skipped (%d start not at 0,0)\n",
         tested, passed, tested - passed, skipped, notStart00);
  if (scored) {
    printf("=== fast route optimal in %d/%d (%.1f%%); mean cost ratio %.3f; worst %.3f (%s)\n",
           optimal, scored, 100.0 * optimal / scored, sumRatio / scored, worstRatio, worstName.c_str());
    printf("=== mean search cells before speed run %.0f; mean runs to first speed run %.2f\n",
           (double)sumSearch / scored, (double)sumRuns / scored);
  }
  return (tested == passed) ? 0 : 1;
}
