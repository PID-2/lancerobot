# Micromouse — Dublin Micromouse Open 2026

Maze-solving firmware for the classic 16×16 UKMARS maze on the competition kit
(ESP32-C6-DevKitC-1, DFRobot DRI0044 driver, 2× GA12-N20 encoded motors, 3× VL53L0X,
SEN0142 IMU, 2S pack through a 5 V buck). Written to the event's Resource Hub and the rules it
links; every hardware number in the code cites its source in `docs/`.

```
solver/solver.h              the algorithm: map, flood fill, turn-weighted planner, run controller
harness/harness.cpp          offline test: drives the solver through 522 real contest mazes
mms/mms_adapter.cpp          runs the solver inside the mms simulator (GUI)
mms/fake_mms.py              headless stand-in for mms, for testing the adapter
esp32/micromouse_esp32/      Arduino sketch for the ESP32-C6: run control + hardware layer
esp32/shim/                  desktop test of the sketch (virtual clock, scripted button and console)
docs/resource-hub-digest.md  everything the Resource Hub and the rules say that affects this code
docs/pinout.md               the team pin map (mirror of config.h) with a verification checklist
docs/reference/              raw copies: hub pages, wiring diagrams, pinout, schematics, datasheets, rules
mazefiles/classic/*.txt      522 contest mazes (github.com/micromouseonline/mazefiles)
```

## Test results (2026-09-23)

```
harness:    522 tested, 522 passed, 0 failed
            fast route optimal in 498/520 (95.8%); mean cost ratio 1.002; worst 1.128
            mean search cells before speed run 357; mean runs to first speed run 3.28
fake_mms:   522 mazes, 0 crashes, every run ends back at the start cell
desktop:    sketch dry run replays a slot on a virtual clock, 15/15 checks pass
esp32 core: sketch compiles for ESP32C6 Dev Module (core 3.3.12) in both configurations
```

- No crash into a real wall on any maze; no move ever proposed through an unobserved side.
- Two mazes (001, 001-anomaly-test) have a sealed centre, which the rules do not allow; the
  solver stops rather than wandering.
- "Optimal" means the planned fast route costs the same as the best route with the whole maze
  known, under the turn-weighted cost model. The 4% that aren't optimal hit the exploration
  budget (`maxExploreLoops = 3`); at 5 loops it is 99.4% but takes ~4% more search driving.

## How it works

**Map** — `walls[256]` and `known[256]`, one bit per side. Setting a wall sets it on both
neighbours. Outer boundary and the three start-cell walls are known at power-up (UKMARS: the
start cell is a corner, open to the north); nothing else is.

**Search run** — from the start, repeatedly plan the cheapest route to the centre with unseen
walls assumed *open* (the optimism rule) and take its first move. Exploration is therefore spent
on the cells that could actually shorten the fast run. Falls back to plain flood fill if the
planner finds nothing. On reaching the centre, flood back to the start (the return leg sees walls
from the other side for free). The rules allow this: a run is timed to the centre and the mouse
may keep searching afterwards.

**Route proven?** — back at the start, plan twice: unknowns open vs unknowns walled. If the two
costs match, nothing unexplored can beat what we know; plan the fast route and stop searching.
Otherwise search again, up to `maxExploreLoops`, then race with what we have.

**Planner** — Dijkstra over 1024 states (cell × heading). Costs are `STRAIGHT = 1.0`,
`TURN90 = 3.5`, `TURN180 = 6.0` (tenths, integers). **Set TURN90 from measurement**: time ten
straight cells against ten cells with a turn in each. The fast run uses the pessimistic map, so
it only ever crosses sides the mouse has seen open, and it is re-planned from the latest map at
the start of every run, so a wall first seen during a speed run is routed around next time.

**Moves** — `F` forward one cell; `L`/`R` turn 90° then one cell; `B` turn 180° then one cell.
Every move ends at a cell centre. `compressPath()` merges straights into `S<n>` so the motion
layer can accelerate over a run of cells. Diagonals are not implemented (S3: get the orthogonal
run reliable first); the hook is the move string.

**Memory** — the map survives between runs. `forgetMaze()` exists for the rule that a
judge-requested recovery erases the mouse's memory.

## The mouse on the day

The rules forbid a laptop or console on the mouse during the contest, so everything the handler
needs is the board's own BOOT button and RGB LED:

| Gesture | Idle | Running |
|---|---|---|
| short press | start a run after a 1.5 s hands-clear countdown (press again to cancel) | emergency stop: wheels brake, map kept |
| hold 1.5 s, release | next speed level 1 → 5 → 1; the LED blinks the new level | |
| hold 5 s, release | forget the maze (after a judge-requested recovery) | |

| LED | Meaning |
|---|---|
| steady blue | idle; the next run is a search |
| steady green | idle; route proven, the next run is a speed run |
| steady violet | idle; the last speed run reached the centre |
| blinks off N times every 3 s | speed level N for the next speed run |
| yellow flashing | countdown, hands clear |
| white / cyan | running a search / a speed run |
| dim | resting: the rules want the mouse still for 2 s in the start cell |
| red flashing | fault; `s` on the console says why; short press clears it |

Ladder (S2, S5): run 1 search, run 2 bank a speed run at level 1 or 2, then escalate. Only the
best run counts. If the mouse does something odd, `p` the map on the bench first: a phantom wall
is a sensor problem, not a solver problem. Do not edit `solver.h` on the floor.

## Sketch layout and bring-up

`esp32/micromouse_esp32/`:

| File | Owner | Purpose |
|---|---|---|
| `config.h` | integrator | **pin map, limits, calibration constants** — the only file with hardware numbers |
| `solver.h` | nobody on the day | copy of `solver/solver.h`; keep in sync |
| `micromouse_esp32.ino` | integrator | button/LED interface, run state machine, bench serial console |
| `hal.h` | — | what the run control needs from the hardware |
| `hal_dryrun.h` | — | `HARDWARE_READY 0`: fake sensors on a built-in maze, no motors |
| `hal_esp32c6.h` | motion + sensor owners | `HARDWARE_READY 1`: DRI0044, encoders, 3× VL53L0X, SEN0142 |
| `board_ui.h` | — | the on-board button and LED |

**Dry run first.** As shipped (`HARDWARE_READY 0`) the sketch walks the UK 2016 final maze from
a table and prints every move; the button and LED are real. Flash it, press the button, watch
the search, the proof and the speed run. Nothing else needs to be wired.

**Then the hardware.** Set `HARDWARE_READY 1` in `config.h` and follow the calibration checklist
at the top of `hal_esp32c6.h` (motors → encoders → wheel and track measurements → IMU sign →
wall thresholds → gains). Runs refuse to start until the encoder and wheel constants are filled
in; the bench console works before that. Console at 115200:

```
g go   x stop   f forget   p map   s status   1-5 level   ? help
w wall sensors   e encoders   i imu   m motor test (wheels off the table)   c gyro cal   b battery
```

Electrical facts the code relies on (sources in `docs/resource-hub-digest.md`): DRI0044 `VCC`
must be 3V3 and PWM 0 is a short brake, never a coast; the 6 V motors run from an 8.4 V pack, so
the firmware caps duty at 6/8.4 (or by measured battery voltage if the monitor is fitted); the
IMU may answer at 0x69 instead of 0x68 (solder jumper), so both are scanned; never connect USB
and the buck's 5 V rail at the same time.

## Build and test

```
# solver against all mazes (must stay 522/522, 0 crashes)
cd harness && g++ -std=c++11 -O2 -Wall -Wextra -I../solver harness.cpp -o harness && ./harness ../mazefiles/classic/*.txt
./harness -v ../mazefiles/classic/uk2016f.txt          # per-cell trace

# adapter, headless
cd mms && g++ -std=c++11 -O2 -I../solver mms_adapter.cpp -o mms_adapter
python3 fake_mms.py ../mazefiles/classic/uk2016f.txt ./mms_adapter      # expect crashes=0

# adapter, Windows exe
x86_64-w64-mingw32-g++ -std=c++11 -O2 -static -I../solver mms_adapter.cpp -o mms_adapter.exe

# sketch, desktop replay of a whole slot (no Arduino toolchain needed); exit code 0 = all checks pass
cd esp32/shim && g++ -std=c++11 -O2 -Wall -Wextra -x c++ -I. -I../micromouse_esp32 main.cpp -o dryrun && ./dryrun

# sketch, real toolchain (Arduino IDE: board "ESP32C6 Dev Module", core esp32 by Espressif 3.x;
# libraries VL53L0X by Pololu, MPU6050 by Electronic Cats)
arduino-cli compile --fqbn esp32:esp32:esp32c6 esp32/micromouse_esp32
arduino-cli compile --fqbn esp32:esp32:esp32c6 --build-property "compiler.cpp.extra_flags=-DHARDWARE_READY=1" esp32/micromouse_esp32
```

## Using it in the mms simulator

1. Get mms from github.com/mackorone/mms (prebuilt releases for Windows/Mac; build from source on Linux).
2. Config: Directory = `mms/`, Build = `g++ -std=c++11 -O2 -I../solver mms_adapter.cpp -o mms_adapter`,
   Run = `./mms_adapter` (Windows: `mms_adapter.exe`).
3. Load any maze from `mazefiles/classic`. Visited cells go cyan; when the route is proven the fast
   route is drawn green and the console says so. Press **Reset** in mms to start each new run,
   exactly like putting the mouse back at the start. The map is kept across resets; restart the
   algorithm for a fresh map.
