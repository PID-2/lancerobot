// mms_adapter.cpp — run the solver inside the mms simulator (github.com/mackorone/mms).
//
// mms talks to the algorithm over stdin/stdout. This file is the only thing
// that knows about that; solver.h is untouched.
//
// Build:  g++ -std=c++11 -O2 -I../solver mms_adapter.cpp -o mms_adapter
// In mms: click "+" (new algorithm): Directory = this folder, Build = (the line above),
//         Run = ./mms_adapter    (Windows: mms_adapter.exe)
//
// Behaviour: search to the centre, return, repeat until the route is proven,
// then run the fast route. Pressing Reset in mms starts a new run with the map
// kept (like a real slot). Load a new maze and restart the algorithm for a
// fresh map. Visited cells are cyan; the planned fast route is green.

#include "solver.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>
#ifdef _WIN32
#include <windows.h>
static void sleepMs(int ms) { Sleep(ms); }
#else
#include <unistd.h>
static void sleepMs(int ms) { usleep(ms * 1000); }
#endif

using namespace mm;

static std::string ask(const std::string& cmd) {
  std::cout << cmd << std::endl;
  std::string r; std::getline(std::cin, r);
  return r;
}
static void tell(const std::string& cmd) { std::cout << cmd << std::endl; }
static bool askBool(const std::string& cmd) { return ask(cmd) == "true"; }
static void log(const std::string& s) { std::cerr << s << std::endl; }

// ASCII dump of the mouse's map: '?' = side never observed.
static void dumpMap(const Mouse& mouse) {
  const Maze& m = mouse.maze;
  std::string out;
  for (int y = mm::SIZE - 1; y >= 0; --y) {
    for (int x = 0; x < mm::SIZE; ++x) { uint8_t c = cellIndex(x, y); out += 'o'; out += !m.isKnown(c, N) ? " ? " : m.hasWall(c, N) ? "---" : "   "; }
    out += "o\n";
    for (int x = 0; x < mm::SIZE; ++x) {
      uint8_t c = cellIndex(x, y);
      out += !m.isKnown(c, W) ? '?' : m.hasWall(c, W) ? '|' : ' ';
      out += ' '; out += (c == mouse.cell) ? 'M' : (m.isGoal(c) && m.goalCount == 4) ? 'G' : ' '; out += ' ';
    }
    out += "|\n";
  }
  for (int x = 0; x < mm::SIZE; ++x) out += "o---";
  out += "o\n";
  std::cerr << out;
}

static void drawWalls(const Mouse& m) {
  uint8_t c = m.cell;
  for (uint8_t d = 0; d < 4; ++d)
    if (m.maze.isKnown(c, (Dir)d) && m.maze.hasWall(c, (Dir)d))
      tell("setWall " + std::to_string(cellX(c)) + " " + std::to_string(cellY(c)) + " " + (char)("nesw"[d]));
}

static void drawFastPath(const Mouse& m) {
  uint8_t c = cellIndex(0, 0); Dir h = N;
  for (uint16_t i = 0; i < m.fastPath.length; ++i) {
    h = headingAfter(h, m.fastPath.moves[i]);
    c = cellIndex((uint8_t)(cellX(c) + dx(h)), (uint8_t)(cellY(c) + dy(h)));
    tell("setColor " + std::to_string(cellX(c)) + " " + std::to_string(cellY(c)) + " g");
  }
}

int main() {
  if (std::atoi(ask("mazeWidth").c_str()) != mm::SIZE || std::atoi(ask("mazeHeight").c_str()) != mm::SIZE) {
    log("this solver is for 16x16 classic mazes"); return 1;
  }
  Mouse mouse; mouse.begin();
  int run = 1;
  log("run 1: search");

  for (;;) {
    if (askBool("wasReset")) {
      ask("ackReset");   // mms replies "ack"; it must be consumed or every later reply is off by one
      mouse.startRun(); ++run;
      log("run " + std::to_string(run) + ": " + (mouse.phase == PHASE_SPEED_RUN ? "SPEED" : "search"));
      continue;
    }

    bool l = askBool("wallLeft"), f = askBool("wallFront"), r = askBool("wallRight");
    {
      // If a side we already recorded disagrees with the live reading, our
      // believed pose has drifted from the simulator's. Say so immediately.
      uint8_t c = mouse.cell; Dir h = mouse.heading;
      Dir sides[3] = { leftOf(h), h, rightOf(h) }; bool live[3] = { l, f, r };
      for (int i = 0; i < 3; ++i)
        if (mouse.maze.isKnown(c, sides[i]) && mouse.maze.hasWall(c, sides[i]) != live[i])
          log("MAP MISMATCH at (" + std::to_string(cellX(c)) + "," + std::to_string(cellY(c)) + ") side " + dirChar(sides[i]) +
              ": map says " + (mouse.maze.hasWall(c, sides[i]) ? "wall" : "open") + ", simulator says " + (live[i] ? "wall" : "open"));
    }
    mouse.observe(l, f, r);
    drawWalls(mouse);
    tell("setColor " + std::to_string(cellX(mouse.cell)) + " " + std::to_string(cellY(mouse.cell)) + " c");

    Move m = mouse.next();
    if (m == MOVE_STOP) {
      if (mouse.phase == PHASE_DONE) {
        tell("setText 8 8 DONE");
        log("speed run finished: " + std::to_string(mouse.fastPath.length) + " moves. Press Reset in mms to run it again.");
      } else if (mouse.phase == PHASE_SPEED_RUN) {
        log("route proven after " + std::to_string(mouse.exploreLoops) + " search loop(s); fast route " + std::to_string(mouse.fastPath.length) + " moves");
        drawFastPath(mouse);
        log("back at start. Press Reset in mms for the speed run.");
      } else {
        log("search loop " + std::to_string(mouse.exploreLoops) + " done, route not yet proven. Press Reset in mms for the next search run.");
      }
      // Wait for the Reset button. Poll gently: mms handles every command on
      // its GUI thread, so a tight loop here floods it.
      while (!askBool("wasReset")) sleepMs(100);
      ask("ackReset");   // mms replies "ack"; it must be consumed or every later reply is off by one
      mouse.startRun(); ++run;
      log("run " + std::to_string(run) + ": " + (mouse.phase == PHASE_SPEED_RUN ? "SPEED" : "search"));
      continue;
    }

    log("(" + std::to_string(cellX(mouse.cell)) + "," + std::to_string(cellY(mouse.cell)) + ") " + dirChar(mouse.heading) +
        " L" + (l ? "1" : "0") + " F" + (f ? "1" : "0") + " R" + (r ? "1" : "0") + " -> " + moveChar(m));
    if (!mouse.moveIsSafe(m)) {
      Dir d = headingAfter(mouse.heading, m);
      log(std::string("BUG: move ") + moveChar(m) + " would cross side " + dirChar(d) + " which the map has as " +
          (!mouse.maze.isKnown(mouse.cell, d) ? "UNKNOWN" : "WALL") + ". Map dump:");
      dumpMap(mouse);
      return 2;
    }
    switch (m) {
      case MOVE_LEFT:  ask("turnLeft");  break;
      case MOVE_RIGHT: ask("turnRight"); break;
      case MOVE_BACK:  ask("turnLeft"); ask("turnLeft"); break;
      default: break;
    }
    std::string resp = ask("moveForward");
    if (resp == "crash") { log("CRASH reported by mms at (" + std::to_string(cellX(mouse.cell)) + "," + std::to_string(cellY(mouse.cell)) + ")"); return 3; }
    mouse.applyMove(m);
  }
}
