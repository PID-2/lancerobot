# Micromouse solver — Dublin Micromouse Open 2026

Maze-solving code for the classic 16×16 UKMARS maze, written to the rules and
guidance in the event's Resource Hub (S1–S5, H1, C2, C3). The solver is
hardware-independent and has been run against 522 real contest mazes.

```
solver/solver.h              the algorithm: map, flood fill, turn-weighted planner, run controller
harness/harness.cpp          offline test: drives the solver through real maze files
mms/mms_adapter.cpp          runs the solver inside the mms simulator (GUI)
mms/fake_mms.py              headless stand-in for mms, for testing the adapter
esp32/micromouse_esp32/      Arduino sketch for the ESP32-C6 (hardware layer stubbed)
mazefiles/classic/*.txt      522 contest mazes (github.com/micromouseonline/mazefiles)
```

## Test results

```
=== 522 tested, 522 passed, 0 failed
=== fast route optimal in 498/520 (95.8%); mean cost ratio 1.002; worst 1.128
=== mean search cells before speed run 357; mean runs to first speed run 3.28
```

- No crashes into a real wall on any maze; no move ever proposed through an unobserved side.
- Two mazes (001, 001-anomaly-test) have a sealed centre; the solver stops rather than wandering.
- "Optimal" means the planned fast route costs the same as the best route with the whole maze known,
  under the turn-weighted cost model. The 4% that aren't optimal hit the exploration budget
  (`maxExploreLoops = 3`); at 5 loops it is 99.4% but takes ~4% more search driving.

Reproduce: `cd harness && g++ -std=c++11 -O2 -I../solver harness.cpp -o harness && ./harness ../mazefiles/classic/*.txt`
Trace one maze: `./harness -v ../mazefiles/classic/uk2016f.txt`

## How it works

**Map** — `walls[256]` and `known[256]`, one bit per side. Setting a wall sets it on both
neighbours. Outer boundary and the three start-cell walls are known at power-up; nothing else is.

**Search run** — from the start, repeatedly plan the cheapest route to the centre with unseen
walls assumed *open* (the optimism rule) and take its first move. Exploration is therefore spent
on the cells that could actually shorten the fast run. Falls back to plain flood fill if the
planner finds nothing. On reaching the centre, flood back to the start (the return leg sees walls
from the other side for free).

**Route proven?** — back at the start, plan twice: unknowns open vs unknowns walled. If the two
costs match, nothing unexplored can beat what we know; plan the fast route and stop searching.
Otherwise search again, up to `maxExploreLoops`, then race with what we have.

**Planner** — Dijkstra over 1024 states (cell × heading). Costs are `STRAIGHT = 1.0`,
`TURN90 = 3.5`, `TURN180 = 6.0` (tenths, integers). **Set TURN90 from measurement**: time ten
straight cells against ten cells with a turn in each. The fast run uses the pessimistic map, so
it only ever crosses sides the mouse has seen open.

**Moves** — `F` forward one cell; `L`/`R` turn 90° then one cell; `B` turn 180° then one cell.
Every move ends at a cell centre. `compressPath()` merges straights into `S<n>` so the motion
layer can accelerate over a run of cells. Diagonals are not implemented (S3: get the orthogonal
run reliable first); the hook is the move string.

**Memory** — the map survives between runs. `forgetMaze()` exists for the rule that a
judge-requested recovery erases the mouse's memory.

## Using it on the ESP32

`esp32/micromouse_esp32/micromouse_esp32.ino` + `solver.h` in the same folder. Board:
ESP32-C6-DevKitC-1, 115200 baud.

With `HARDWARE_READY 0` (as shipped) it is a **dry run**: no motors or sensors, it walks the
UK 2016 final maze from a built-in table and prints every move. Flash it today; send `g`
four times and `p` to see the search, the proof, the speed run and the map.

To go live, set `HARDWARE_READY 1` and fill in the hardware layer:

| Function | Owner | Contract |
|---|---|---|
| `readWalls(l, f, r)` | sensors | three VL53L0X, fixed order, timeouts rejected, hysteresis, sampled at cell centre; return `false` if invalid |
| `forwardOneCell(level)` | motion | 180 mm by encoders, trapezoid profile, side-wall centring PID, hold heading if a side wall vanishes, front-wall distance reset |
| `turnLeft / turnRight / turnAround(level)` | motion | pivot by gyro + encoders; arc turns later without touching the solver |
| `calibrateGyro()` | sensors | mouse still at power-up, average zero-rate offset |

`level` 1–5 is the run ladder (S2): search always runs at 1; each speed run uses the level set
over serial (`1`–`5`). Bank a finish at 1 or 2 before raising it.

Serial console: `g` run, `f` forget maze, `p` print map (`?` = side never observed — this is how
you spot a phantom wall), `s` status, `1`–`5` speed level for the next speed run.

Pin map is the block at the top of the sketch. Keep it as the single source of truth and mirror
it in `docs/pinout.md` for anyone using an AI agent on the firmware (C3).

Cost on the board: `plan()` is ~0.17 ms on a desktop (worst case, empty map); expect ~10 ms per
cell on the C6. It runs once per cell during search, which is far shorter than the drive.
The Dijkstra scratch (~5 KB) is static so it stays off the task stack.

## Using it in the mms simulator

1. Get mms from github.com/mackorone/mms (prebuilt releases for Windows/Mac; build from source on Linux).
2. Config: Directory = `mms/`, Build = `g++ -std=c++11 -O2 -I../solver mms_adapter.cpp -o mms_adapter`,
   Run = `./mms_adapter` (Windows: `mms_adapter.exe`).
3. Load any maze from `mazefiles/classic`. Visited cells go cyan; when the route is proven the fast
   route is drawn green and the console says so. Press **Reset** in mms to start each new run,
   exactly like putting the mouse back at the start. The map is kept across resets; restart the
   algorithm for a fresh map.

Headless check without the GUI: `python3 fake_mms.py ../mazefiles/classic/uk2016f.txt ./mms_adapter`

## On the day (S5)

- Hour 3–4: set `HARDWARE_READY 1`, run at level 1. Reaching the centre once makes you legal.
- Do not edit `solver.h` on the floor. If the mouse does something odd, `p` the map first: a
  phantom wall is a sensor problem, not a solver problem.
- Measure `TURN90` before the speed runs; it changes which route the planner picks.
- Ladder: run 1 search, run 2 bank a speed run at level 1–2, then escalate. Only the best run counts.
