# HANDOFF — Micromouse solver (Dublin Micromouse Open, Sat 26 Sep 2026)

Read this first. It is the complete state of the project as of 2026-09-23. `README.md` is the
user-facing doc; this file is for the next Claude Code session.

## Working rules (from Troy — non-negotiable)

- Read-only by default. No writes, edits, deletes, commits, pushes, flashes or external calls
  without explicit permission for that specific action. Flag clearly anything that would alter state.
- Discuss before building: show the design, let him veto, then write code.
- Time-box every task to 1–2 hours with a hard stop. No all-day chains unless he agrees.
- Never guess hardware facts. Pin numbers, register maps, protocol details come from the
  datasheet or the event's Resource Hub pages, never from memory. If unsure, ask.
- Terse, direct output. No hedging, no restating the request.

## What this is

A maze solver for the classic 16×16 UKMARS micromouse maze, plus the scaffolding to test it
without a robot and to drop it onto the competition ESP32-C6. Event rules and kit are documented
at https://hackclub.ucdelecsoc.com/micromouse-resources.html (pages S1–S6, H1–H6, C2–C3).
Format: every team gets the same kit and six hours on the day; fastest verified run to the centre
wins; 10 minutes and up to 5 runs per slot; best single run counts; map persists between runs.

## Layout

```
solver/solver.h                  THE ALGORITHM. Header-only C++11, no heap/STL. Tested. Do not edit casually.
harness/harness.cpp              Offline test: perfect-sensor mouse through real maze files.
mms/mms_adapter.cpp              Adapter for the mms simulator (github.com/mackorone/mms), stdin/stdout protocol.
mms/fake_mms.py                  Headless stand-in for mms; used to sweep the adapter over all mazes.
mms/mms_adapter.exe              Windows build of the adapter (x86_64-w64-mingw32-g++, -static). Troy runs this.
esp32/micromouse_esp32/*.ino     Arduino sketch. Hardware layer is STUBS. Ships in dry-run mode.
esp32/micromouse_esp32/solver.h  Copy of solver/solver.h (Arduino needs it beside the .ino). Keep in sync.
esp32/shim/                      Fake Arduino.h + main.cpp so the .ino can be compiled with g++ on a desktop.
mazefiles/classic/*.txt          522 contest mazes from github.com/micromouseonline/mazefiles.
```

## Architecture (solver.h)

Three layers, one seam. The solver never touches hardware.

- `Maze` — `walls[256]`, `known[256]` (one bit per side N/E/S/W), cell index `x + 16*y`, (0,0)
  bottom-left, y up. `setWall()` updates both neighbours. `reset()` marks the boundary and the three
  start-cell walls known. `flood(unknownOpen)` is BFS from the goal cells; `passable()` treats
  unseen sides as open (optimistic) or walled (pessimistic).
- `Planner` — Dijkstra over 1024 (cell, heading) states. Costs in tenths: `STRAIGHT=10`,
  `TURN90=35`, `TURN180=60`. O(V²) selection, ~0.17 ms desktop worst case; scratch arrays are
  function-static (~5 KB) to stay off the ESP32 task stack. Not reentrant.
- `Mouse` — run controller. Phases: `SEARCH_OUT` (each cell: plan optimistic route to centre, take
  its first move; fallback plain flood) → `SEARCH_BACK` (flood to start) → at start, `routeProven()`
  = optimistic plan cost == pessimistic plan cost → if proven or `maxExploreLoops` (3) reached,
  `planSpeedRun()` on the pessimistic map → `SPEED_RUN` replays `fastPath` → `DONE`.
  `startRun()` resets pose to (0,0) facing N and keeps the map. `forgetMaze()` = full reset.

Contract with the outside world:
```
mouse.observe(left, front, right)   // relative wall readings at the cell centre
Move m = mouse.next()               // F, L, R, B, or STOP (run over)
execute m; mouse.applyMove(m)
mouse.moveIsSafe(m)                 // the side about to be crossed is known open
```
Move semantics: `F` = forward one cell; `L`/`R` = turn 90° then forward one cell; `B` = 180° then
forward one. Every move ends at a cell centre. `compressPath()` gives `S<n>`/`L`/`R`/`B` segments
so a motion layer can accelerate over straights. Diagonals are deliberately not implemented.

## Test status

Harness, all 522 classic mazes, 2026-09-22:
```
522 tested, 522 passed, 0 failed
fast route optimal in 498/520 (95.8%); mean cost ratio 1.002; worst 1.128 (sd2p02)
mean search cells before speed run 357; mean runs to first speed run 3.28
```
- "Optimal" = planned fast route cost equals the best route with the full maze known.
- The 4% non-optimal hit `maxExploreLoops=3`; at 5 it is 99.4% with ~4% more search driving.
- Two mazes (001, 001-anomaly-test) have a sealed centre; the solver stops without crashing.
- fake_mms sweep over all 522 mazes: 0 crashes.
- mms GUI on Windows: run 1 search out and back proven on the first loop (100-move route).
  Speed run confirmed after the ackReset fix below. (Troy last reported the fixed exe was sent;
  confirm with him that the speed run completed in mms.)

## Bugs found and fixed (so you don't reintroduce them)

1. **mms replies `ack` to `ackReset`.** The adapter originally sent it with no read; every reply
   after a Reset was then off by one line and the wall readings were garbage. Fixed: `ask("ackReset")`.
   `fake_mms.py` now replies `ack` too. Rule: every mms command that returns something must be read.
2. **Busy-wait on `wasReset` floods the mms GUI thread.** Now polls every 100 ms.
3. **`SIZE` clashes with `windows.h`** when the adapter is built with MinGW. Use `mm::SIZE`.
4. Initial `routeProven()` compared BFS cell counts, not turn-weighted cost → 73% optimal. Now
   compares planner costs → 95.8%.

## How to build and test

```
# solver against all mazes (must stay 522/522, 0 crashes)
cd harness && g++ -std=c++11 -O2 -Wall -Wextra -I../solver harness.cpp -o harness && ./harness ../mazefiles/classic/*.txt
./harness -v ../mazefiles/classic/uk2016f.txt          # per-cell trace

# adapter, headless
cd mms && g++ -std=c++11 -O2 -I../solver mms_adapter.cpp -o mms_adapter
python3 fake_mms.py ../mazefiles/classic/uk2016f.txt ./mms_adapter      # expect crashes=0

# adapter, Windows exe (Troy has no local compiler)
x86_64-w64-mingw32-g++ -std=c++11 -O2 -static -I../solver mms_adapter.cpp -o mms_adapter.exe

# sketch, desktop compile-check + dry run (no Arduino toolchain needed)
cd esp32/shim && g++ -std=c++11 -O2 -Wall -Wextra -x c++ -I. -I../micromouse_esp32 main.cpp -o dryrun && ./dryrun
```
mms config on Troy's Windows box: Directory = `...\micromouse\mms`, Build = empty,
Run = full path to `mms_adapter.exe`. Reset button in mms = new run, map kept.

The sketch has NOT been compiled with the real ESP32 Arduino core (toolchain unreachable from the
build environment). Expect trivial fixes on first real compile — `Serial.printf`, `PROGMEM`,
`pgm_read_byte` are all standard on the ESP32 core, so nothing structural.

## Open work, in priority order

1. **Hardware layer** in the .ino (motion owner + sensor owner on the team, not necessarily Troy):
   `readWalls`, `forwardOneCell`, `turnLeft/Right/Around`, `calibrateGyro`. Contracts are in the
   README table. Kit per the Resource Hub: ESP32-C6-DevKitC-1, DFRobot DRI0044 driver, SEN0142
   IMU (MPU-6050 @0x68), 3× VL53L0X (re-address to 0x30/31/32 via XSHUT every boot), 2S→5 V buck,
   2× GA12-N20 encoded motors. Pin map block at the top of the sketch is the single source of truth
   and is currently placeholder (button GPIO2, LED GPIO7). Avoid strapping pins 4,5,8,9,15 and USB
   12,13. Check every pin against the DevKitC-1 pinout PNG on page H1 — never from memory.
2. **Measure `TURN90`** on the real mouse (ten straight cells vs ten cells with a turn each) and set
   it in `Planner`. It changes which route is chosen.
3. **Confirm `plan()` time on the C6** with `micros()`. Estimate ~10 ms/cell. If it matters, replace
   the O(V²) selection with a bucket queue (max edge weight is 70 tenths).
4. Optional: raise `maxExploreLoops` to 5 if search runs are fast enough on the real maze.
5. Optional, after everything above is reliable: diagonal rewriting of `L R L R` runs (S3).

## Things not to do

- Don't refactor `solver.h` without re-running the 522-maze harness.
- Don't add STL containers or heap allocation to `solver.h`; it must stay Arduino-safe.
- Don't change move semantics (`L` = turn + one cell) — the sketch, adapter and harness all assume it.
- Don't touch Troy's server, repos or hardware without permission for that specific action.
