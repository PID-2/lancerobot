// Desktop test of the sketch in its dry-run configuration. Replays a whole
// contest slot on a virtual clock: runs started from the serial console and
// from the button, a level change, an emergency stop, a fault clear and a
// forget-maze gesture. Exit code 0 means every check passed.
//
// Build and run (from esp32/shim):
//   g++ -std=c++11 -O2 -Wall -Wextra -x c++ -I. -I../micromouse_esp32 main.cpp -o dryrun && ./dryrun
#include "Arduino.h"
#include <stdlib.h>

uint64_t  shimClockUs = 0;
int       shimButtonPin = -1;
ShimPress shimPresses[8];
int       shimPressCount = 0;
uint8_t  shimLedR = 0, shimLedG = 0, shimLedB = 0;
int      shimLedWrites = 0;
SerialT  Serial;

#include "../micromouse_esp32/micromouse_esp32.ino"

static int failures = 0;
static void check(bool ok, const char* what) {
  printf("[%s] %s\n", ok ? " ok " : "FAIL", what);
  if (!ok) ++failures;
}

// Run loop() until `ms` of virtual time have passed.
static void runFor(uint32_t ms) {
  const uint64_t until = shimClockUs + (uint64_t)ms * 1000;
  while (shimClockUs < until) { loop(); shimClockUs += 1000; }
}

// Run loop() until the state machine has left IDLE and come back to it (or the time limit passes).
static void runUntilIdle(uint32_t limitMs) {
  const uint64_t until = shimClockUs + (uint64_t)limitMs * 1000;
  while (state == ST_IDLE && shimClockUs < until) { loop(); shimClockUs += 1000; }
  while (state != ST_IDLE && shimClockUs < until) { loop(); shimClockUs += 1000; }
  runFor(50);   // one more pass so the LED reflects the new state
}

int main() {
  shimButtonPin = PIN_BUTTON;
  setup();
  runFor(50);
  check(state == ST_IDLE, "boots to IDLE");
  check(shimLedB > 0 && shimLedG < 60, "idle LED is blue (search next)");

  // 1. Serial 'g': first search run goes out and comes back.
  Serial.type("g"); runUntilIdle(60000);
  check(runNumber == 1 && lastRunEnd == RUN_FINISHED, "run 1 (search) finished from the console");
  check(mouse.cell == cellIndex(0, 0), "run 1 ended back in the start cell");

  // 2. Hold 1.8 s: speed level 1 -> 2.
  shimPressButton(1800); runFor(2500);
  check(speedLevel == 2, "long press moved the speed level to 2");

  // 3. Short press: countdown then run 2 (search, should prove the route on this maze).
  shimPressButton(200); runUntilIdle(60000);
  check(runNumber == 2 && lastRunEnd == RUN_FINISHED, "run 2 started by a short press");
  check(mouse.speedRunReady, "route proven after run 2");
  check(shimLedG > 0 && shimLedB == 0, "idle LED is green (speed run next)");

  // 4. Press during the countdown cancels it.
  shimPressButton(200); runFor(600); shimPressButton(200); runFor(2000);
  check(state == ST_IDLE && runNumber == 2, "press during the countdown cancels the start");

  // 5. Short press: the speed run reaches the centre.
  shimPressButton(200); runUntilIdle(60000);
  check(runNumber == 3 && mouse.phase == PHASE_DONE, "run 3 speed run reached the centre");
  check(shimLedR > 0 && shimLedB > 0, "idle LED is violet (centre reached)");

  // 6. Emergency stop: a short press starts run 4, a second press 1 s into the run stops it.
  shimPressButton(200); shimPressButton(400, 200 + START_DELAY_MS + 1000); runUntilIdle(60000);
  check(runNumber == 4 && lastRunEnd == RUN_ABORTED, "button press during run 4 stopped it");
  check(mouse.speedRunReady, "map and route survive an emergency stop");

  // 7. Fault path: a run without dryRunAttach cannot happen here, so simulate a fault clear.
  fault("simulated fault"); runFor(10);
  shimPressButton(200); runFor(1000);
  check(state == ST_IDLE, "short press clears a fault");

  // 8. Hold 5.5 s: forget the maze.
  shimPressButton(5500); runFor(7000);
  check(!mouse.speedRunReady && mouse.exploreLoops == 0 && runNumber == 0, "5 s hold forgot the maze");

  // 9. Console still works: map and status.
  Serial.type("ps?"); runFor(10);

  printf("\n%d check(s) failed, %d LED writes, virtual time %.1f s\n", failures, shimLedWrites, shimClockUs / 1e6);
  return failures ? 1 : 0;
}
