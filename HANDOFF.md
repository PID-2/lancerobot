# HANDOFF — Micromouse (Dublin Micromouse Open, Sat 26 Sep 2026)

Read this first. It is the complete state of the project as of 2026-09-23 (evening).
`README.md` is the user-facing doc; this file is for the next Claude Code session.

## Working rules (from Troy — non-negotiable)

- Read-only by default. No writes, edits, deletes, commits, pushes, flashes or external calls
  without explicit permission for that specific action. Flag clearly anything that would alter state.
- Discuss before building: show the design, let him veto, then write code.
- Time-box every task to 1–2 hours with a hard stop. No all-day chains unless he agrees.
- Never guess hardware facts. Pin numbers, register maps, protocol details come from the
  datasheet or the event's Resource Hub pages, never from memory. If unsure, ask.
- Terse, direct output. No hedging, no restating the request. He will ask you to "dumb it down";
  plain yes/no questions work best.

## Decisions Troy has already taken (do not re-ask)

1. The project lives in github.com/PID-2/lancerobot, branch `claude/zen-wozniak-74804d` (moved on
   2026-09-24 from hfhfkjh/lancerobot, branch `claude/new-session-eg03xa`, which is one commit behind).
2. Start button and status LED are the ones on the DevKitC-1 (BOOT on GPIO9, RGB LED on GPIO8).
3. The mouse is operated by button and LED only in the slot (rules forbid a console); the run
   control was redesigned for that.
4. Motor voltage is limited in software (duty cap 6 V / 8.4 V), no extra regulator.

## What this is

A maze solver for the classic 16×16 UKMARS maze, the scaffolding to test it without a robot,
and the ESP32-C6 firmware around it. The event site is https://hackclub.ucdelecsoc.com/micromouse-resources.html;
everything it and the linked rules/datasheets say that matters is condensed in
`docs/resource-hub-digest.md`, with raw copies in `docs/reference/`. Format: every team gets the
same kit and six hours; 10 minutes and up to 5 runs per slot (judges may shorten); best single
run counts; map persists between runs; a manual recovery requested by the handler erases the map.

## Layout

```
solver/solver.h                  THE ALGORITHM. Header-only C++11, no heap/STL. Tested. Do not edit casually.
harness/harness.cpp              Offline test: perfect-sensor mouse through real maze files.
mms/mms_adapter.cpp              Adapter for the mms simulator; mms_adapter.exe is Troy's Windows build.
mms/fake_mms.py                  Headless stand-in for mms; sweeps the adapter over all mazes.
esp32/micromouse_esp32/          Arduino sketch: config.h (pins/limits), run control (.ino), hal_dryrun.h, hal_esp32c6.h
esp32/micromouse_esp32/solver.h  Copy of solver/solver.h. Keep in sync (diff them).
esp32/shim/                      Desktop test of the sketch: virtual clock, scripted button and console, 15 checks.
docs/                            resource-hub-digest.md, pinout.md, reference/ (site pages, diagrams, datasheets, rules)
mazefiles/classic/*.txt          522 contest mazes.
```

## Architecture

**solver.h** (unchanged apart from one line, see below): `Maze` (walls/known bits, flood),
`Planner` (Dijkstra over cell×heading, costs STRAIGHT=10, TURN90=35, TURN180=60 tenths),
`Mouse` (SEARCH_OUT → SEARCH_BACK → proven? → SPEED_RUN → DONE). Contract:
`observe(l,f,r)`, `next()`, `applyMove(m)`, `moveIsSafe(m)`. `startRun()` now re-plans the fast
route from the latest map each run and drops back to search if no route is left.

**Sketch** (rewritten this session):
- `config.h` — pin map (proposal, see docs/pinout.md), electrical limits, CALIBRATE constants,
  run-control timings. `HARDWARE_READY` lives here (0 as shipped).
- `micromouse_esp32.ino` — state machine IDLE → COUNTDOWN (1.5 s) → RUNNING → REST (2 s, a rule)
  → IDLE, plus FAULT. Button gestures: short = go / stop / clear fault, 1.5 s = next level,
  5 s = forget. LED colours documented in the README. `runAbortRequested()` is polled by every
  hardware move so the button stops the wheels within ~50 ms. Bench console kept for the bench.
- `hal.h` — the interface; `hal_dryrun.h` — fake sensors on uk2016f, real button/LED;
  `hal_esp32c6.h` — motors (LEDC 20 kHz, duty cap), quadrature encoders on interrupts, three
  VL53L0X booted through XSHUT to 0x30/31/32 with non-blocking continuous reads and
  hysteresis, MPU-6050 at 0x68 or 0x69 with boot-time bias calibration, forwardOneCell
  (trapezoid on encoders, wall centring, gyro heading hold, front-wall reset), pivot turns by
  gyro with encoder cross-check, bench commands w/e/i/m/c/b, calibration refusal until the
  encoder and wheel constants are set.

Hardware facts the firmware relies on and where they came from: DRI0044 schematic (STBY tied
high, DIR through an inverter → PWM 0 = short brake, VCC must be 3V3 for the input thresholds);
TB6612 datasheet (100 kHz max PWM, 1.2 A); SEN0142 schematic (4.7 kΩ pull-ups on board, AD0 on
a solder jumper → 0x68 or 0x69, VLOGIC 3.3 V); MPU-6000A spec (±500 dps = 65.5 LSB/dps);
H4 page (XSHUT sequence, addresses); Pololu README (API); DevKitC-1 user guide (strapping
4/5/8/9/15, USB 12/13, UART 16/17, RGB LED on 8, power options mutually exclusive).

## Test status (2026-09-23)

```
harness     522 tested, 522 passed, 0 failed; optimal 498/520 (95.8%); mean ratio 1.002; worst 1.128 (sd2p02)
fake_mms    522 mazes, 0 crashes, all runs end at (0,0)
desktop     esp32/shim: 15/15 checks (console run, level change, countdown cancel, speed run to centre,
            emergency stop, fault clear, forget maze), virtual time ~50 s
arduino     compiles clean (--warnings all) for esp32:esp32:esp32c6, core 3.3.12:
            HARDWARE_READY 0: 287 KB flash / 23 KB RAM;  HARDWARE_READY 1: 334 KB / 24 KB
```
Nothing has run on the real mouse. `hal_esp32c6.h` is a first cut from the datasheets: expect
gain tuning and sign flips, not restructuring. The Windows `mms_adapter.exe` predates the solver's
one-line change (replan on startRun); behaviour in mms is unaffected unless a wall is observed
during a speed run, but rebuild it when convenient (mingw is not in this container).

## Bugs found and fixed (so you don't reintroduce them)

1. mms replies `ack` to `ackReset`; it must be read or every later reply is off by one.
2. Busy-wait on `wasReset` floods the mms GUI thread; poll every 100 ms.
3. `SIZE` clashes with `windows.h` under MinGW; use `mm::SIZE`.
4. `routeProven()` must compare planner costs, not BFS cell counts (73% → 95.8% optimal).
5. `PIN_RGB_LED` is a macro in the ESP32-C6 Arduino variant; the sketch's constant is `PIN_STATUS_LED`.
6. The fast route was never re-planned after `speedRunReady`; a wall seen during a speed run
   was run into again next run. `startRun()` now re-plans (harness numbers unchanged).

## How to build and test

See README "Build and test". The ESP32 toolchain is not in the repo. A fresh container installs
it with `esp32/tools/install-arduino.sh`: about 3 minutes, then it compiles the sketch both ways as
a smoke test (about 3 more). It pins arduino-cli 1.5.1, core esp32 3.3.12 and the two libraries at
the tags the sketch was verified with, all under /opt/arduino, and passes `$HTTPS_PROXY` through.
The GitHub releases web page and api.github.com are blocked by the container proxy; release
downloads, espressif.github.io and downloads.arduino.cc are not. That is why versions are pinned.

## Open work, in priority order

1. **Bench bring-up** (team, with the mouse): docs/pinout.md checklist, then the calibration
   checklist at the top of hal_esp32c6.h. Fill the CALIBRATE constants in config.h. Expect to
   flip signs and tune gains. Record ticks/rev, wheel diameter, track width, thresholds.
2. **Measure `TURN90`** on the real mouse (ten straight cells vs ten cells with a turn each) and
   set it in `Planner`. It changes which route is chosen.
3. **Confirm `plan()` time on the C6** with `micros()` (estimate ~10 ms/cell). If it matters,
   replace the O(V²) selection with a bucket queue (max edge weight 70 tenths).
4. Speed runs currently stop at every cell centre (solver contract). Carrying speed across `S<n>`
   straights in `forwardOneCell` is the next real speed gain once level 2 is reliable.
5. Optional: `maxExploreLoops` 5 if search runs are fast enough; diagonals (S3) last.
6. Rebuild `mms/mms_adapter.exe` with MinGW after the solver change.

## Things not to do

- Don't refactor `solver.h` without re-running the 522-maze harness and the fake_mms sweep.
- Don't add STL containers or heap allocation to `solver.h`; it must stay Arduino-safe.
- Don't change move semantics (`L` = turn + one cell) — sketch, adapter and harness all assume it.
- Don't put hardware numbers anywhere but `config.h` (and its mirror `docs/pinout.md`).
- Don't touch Troy's server, repos or hardware without permission for that specific action.
