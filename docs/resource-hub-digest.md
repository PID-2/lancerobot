# Resource Hub digest — hackclub.ucdelecsoc.com, scanned 2026-09-23

Every fact below carries its source. Lines marked **[analysis]** are my reading of the facts,
not something the site says. Raw copies of everything cited are in `reference/` next to this file.

## 1. What was scanned

- All 24 same-host pages reachable from the hub (hub, S1–S6, H1–H6, C2–C3, event page, index,
  wiki/workshop shells, videos, resources) and all 37 assets they reference.
- The wiki bundle `content/content.js` (137 markdown docs). It holds a second, longer set of
  Micromouse guides (`docs/Micromouse2026/Hardware/*.md`, `Software/*.md`) with the same facts as
  the static pages plus datasheet links and code samples. Both sets were read in full.
- Linked references that were reachable: UKMARS Classic rules, DRI0044 schematic and TB6612FNG
  datasheet, SEN0142 schematic, MPU-6000/6050 product spec, DevKitC-1 user guide (Espressif),
  DFRobot wikis for DRI0044 and SEN0142, Pololu VL53L0X library README.
- Blocked by the session's egress policy: `st.com` (VL53L0X datasheet, AN4846). Not fetched.
- Not on the site: the ESP32-C6 register/strapping details beyond the pinout, an MPU-6050
  register map, any GA12-N20 encoder datasheet (H6 links a seller page only).

## 2. Rules that bind the code

Source: S1 page and the UKMARS Classic rules it links (`reference/rules/ukmars-classic-rules.txt`).

| Rule | Effect on this project |
|---|---|
| 10 min per slot, up to 5 runs; judges may cut to 7 min or less (UKMARS) | Handoff's budget is right; `maxExploreLoops` must also fit a 7-min slot **[analysis]** |
| A run is timed start-cell exit → goal entry; best run counts; time between runs still burns the slot | Run ladder as designed |
| Mouse may keep searching after reaching the goal; the run ends when it returns to the start and stops ≥ 2 s (UKMARS) | Search-out-and-back in one run is legal. The mouse must stand still ≥ 2 s at the start before the next run, or that run's time is invalid |
| Manual recovery only with a judge's permission. Recovery for a clear malfunction is allowed; recovery "under any other circumstances" is granted on condition all maze memory is erased (UKMARS). S1 states it simply: a requested recovery erases the map. Dublin adds a penalty where a touch is allowed and the run continues: 3 s + 1/10 of run time | `forgetMaze()` is the right primitive; whether to press it after a malfunction recovery is a judge-by-judge call |
| **No console or development device may be connected to the mouse during the contest, and no program loading (UKMARS "Notes")** | The sketch's serial console (`g`, `1`–`5`, `f`) cannot be used in the slot. Speed level and forget must be reachable from the mouse itself (button / LED) — see finding F1 |
| Start cell is a corner with walls on three sides; with the opening north, outer walls are west and south (UKMARS) | Matches `Maze::reset()` exactly (E wall set, N open) |
| Goal is the central 2×2; no walls or posts inside it; at least one wall touches every post except the centre | Solver assumptions hold. Sealed-centre mazes cannot occur in a legal contest |
| Timing sensors are photoelectric, horizontal, 1 cm above the floor, at the start-cell boundary and every goal entrance; "passed" = all parts within 5 cm of the floor are inside the goal | Mechanical note: the mouse must break a beam 1 cm up **[analysis]** |
| Mouse must fit a 25 cm square at all times; no height limit; nothing left in the maze; no components (batteries included) added, removed or swapped during the competition | S5's "charge between sessions" is the only option |
| Maze: 18 cm cells, 16.8 cm passage, walls 1.2 cm thick and 5 cm high, posts 1.2 cm | `forwardOneCell` = 180 mm, as in the README |
| Dublin format: 6 h build, identical kit, fastest verified run wins, plus design and reliability awards; full-size and mini practice mazes all day, one mini maze laid out so the fastest line is diagonal | Diagonals stay optional for the main race |

## 3. Strategy pages vs the solver

S2 and S3 describe exactly what `solver.h` does: optimistic search to the goal, return leg,
flood twice (unknowns open vs walled) and race when the costs match, Dijkstra over
cell × heading with `STRAIGHT = 1.0`, `TURN_90 = 3.5` set from measurement. `TURN180 = 6.0` is the
project's own value; the site defines no 180° cost. S3 also says "re-flood after every new wall;
an ESP32 does it well under a millisecond" — consistent with the handoff's plan() estimate.

S4/S5 give the motion-layer contract the README table already mirrors: distance-based motion
profiles, pivot turn first, gyro heading with wall correction, side-wall centring PID that holds
heading when a wall vanishes, front-wall distance reset, fixed-rate sensor sampling in a timer,
two-threshold hysteresis, wall reads at a consistent point in the cell. S5 adds two test items the
README does not list: test turning around inside the goal block, and make the start pose
impossible to get wrong when placed in a hurry.

## 4. Kit facts, per component

### H1 ESP32-C6-DevKitC-1 (Espressif user guide + pinout PNG on H1)

Header pins, from the user guide's J1/J3 tables (authoritative, matches the PNG):

| GPIO | Also | Status for the mouse |
|---|---|---|
| 0, 1, 2, 3 | ADC1_CH0–3, LP_UART, (2: FSPIQ) | free; the only free ADC pins besides 6 |
| 4, 5 | MTMS, MTDI, ADC1_CH4/5 | **strapping** — avoid |
| 6, 7 | MTCK/MTDO, LP_I2C_SDA/SCL, ADC1_CH6 on 6 | free (JTAG lost, not needed) |
| 8 | **strapping**; drives the on-board addressable RGB LED | avoid as a header pin |
| 9 | **strapping**; on-board BOOT button | avoid as a header pin |
| 10, 11 | — | free |
| 12, 13 | USB_D−, USB_D+ (native USB port) | avoid |
| 15 | **strapping** | avoid |
| 16, 17 | U0TXD/U0RXD to the USB-UART bridge | keep for the serial monitor |
| 18–23 | SDIO/FSPICS labels only | free |

- Strapping pins per the user guide: MTMS (4), MTDI (5), GPIO8, GPIO9, GPIO15. Same list as the handoff.
- Power: three mutually exclusive sources — USB (either port), the 5V+GND header, the 3V3+GND header. J5 jumper isolates the module for current measurement.
- Chip: RISC-V single core 160 MHz, 512 KB SRAM, LEDC PWM 6 channels, 1 I2C, 12-bit ADC 7 ch (pinout PNG spec box).
- 3.3 V logic only; never 5 V on a GPIO; controller, sensors and driver share one ground (H1).

**[analysis] Pin budget.** Free header GPIOs after the exclusions above: 0, 1, 2, 3, 6, 7, 10, 11, 18,
19, 20, 21, 22, 23 = **14**. Hard requirements: 2 PWM + 2 DIR + 4 encoder + SDA + SCL + 3 XSHUT
= **13**, leaving one spare (IMU INT or battery ADC on 0–3). The sketch's external start button
(GPIO2) and LED (GPIO7) would take two more and overrun the budget by one. Options, for the team:
use the on-board BOOT button (GPIO9) as the start button and the on-board RGB LED (GPIO8) as the
status LED — both are already wired on the board, so the strapping caveat is the board's own
design — or give up the spare, or free 16/17 by monitoring over the native USB port instead.

### H2 DFRobot DRI0044 (H2 page, DFRobot wiki, schematic, TB6612FNG datasheet)

- Ratings: 1.2 A continuous per channel, 3.2 A single-pulse peak; VCC 2.7–5.5 V; VM 2.5–12 V on the module (13.5 V IC operating max). PWM up to 100 kHz.
- Pins: DIR1/PWM1 (M1), PWM2/DIR2 (M2), GND, VCC, M1±, M2±, GND, VM. Wire VCC to the ESP32 **3V3**, share ground, VM from the motor supply.
- Schematic facts: STBY is tied to VCC (always enabled, no pin needed). Each DIR input drives one TB6612 input directly and the other through an SN74LVC2G14 inverter, so IN1 and IN2 are always complementary.
- TB6612 truth table: complementary inputs with PWM high = drive; PWM low = **short brake**. IN1 = IN2 = L (stop/coast) is unreachable on this module.
- **[analysis]** Consequences: duty 0 is a brake, not a coast (good for stopping at cell centres, bad if the motion code assumes free-wheeling). VCC must be 3V3, not 5 V: the datasheet's input threshold is 0.7 × VCC, so at VCC = 5 V a 3.3 V ESP32 signal is out of spec. Direction sense per wheel depends on M+/M− wiring; H2 says swap wires or invert in software.

### H3 DFRobot SEN0142 (H3 page, DFRobot wiki + example, schematic, MPU-6000A spec)

- MPU-6050, I2C, address **0x68** stated on H3; 3–5 V in; power it from 3V3. Gyro ±250/500/1000/2000 °/s (131 LSB/°/s at ±250); accel ±2/4/8/16 g.
- Pins: VIN, GND, SDA, SCL, INT (optional), AUX_DA/AUX_CL (leave open).
- Schematic facts: 4.7 kΩ pull-ups on SDA and SCL to 3.3 V are **on the board**; VLOGIC is tied to 3.3 V, so I2C runs at 3.3 V levels; on-board BL8555-3.3 LDO from VIN; AD0 is pulled up through 470 Ω with a solder jumper J1 to ground.
- **[analysis]** If J1 is open, AD0 is high and the address is **0x69**. H3 says scan and look for 0x68 first; the firmware should accept either. With the SEN0142's 4.7 kΩ plus whatever the three GY-530 boards carry, do not add external pull-ups (H1 says check on-board pull-ups first).
- DFRobot's own example uses Jeff Rowberg's I2Cdev + MPU6050 library (`initialize()`, `testConnection()`, `getMotion6()`, `setXGyroOffset()`). The MPU-6050 register map is a separate document the site does not link.
- Calibration procedure (H3, S4): flat mounting, note the forward edge, average the gyro at rest for a few seconds at power-up with the mouse untouched, confirm the sign of a left turn.

### H4 3× VL53L0X on GY-530 boards (H4 page, wiring SVG, Pololu README)

- All boot at 0x29. At every boot: all XSHUT LOW → enable LEFT, set 0x30 → enable FRONT, set 0x31 → enable RIGHT, set 0x32 → scan shows 0x30/0x31/0x32. Keep the three constants in one place.
- Board pins (photo on H4): VCC, GND, SCL, SDA, GPIO1, XSHUT. GPIO1 unconnected. Check the kit's boards actually expose XSHUT; some four-pin boards do not.
- XSHUT is active-low: LOW holds the sensor in shutdown, HIGH enables it (C3 worked example).
- Pololu library API (README): `init()`, `setAddress()`, `setTimeout()`, `setMeasurementTimingBudget()` (default ≈ 33 ms, minimum 20 ms), `startContinuous()`, `readRangeContinuousMillimeters()`, `timeoutOccurred()`, `setSignalRateLimit()`.
- H4: do not trust the 2 m figure; measure the reliable range on the real walls in the real room; read in a fixed order; reject timeouts and out-of-range values; use hysteresis.
- **[analysis]** Three single-shot reads at 33 ms each is ~100 ms per cell, too slow for centring at speed. Continuous mode on all three with a 20–33 ms budget gives 30–50 Hz. The ST datasheet and AN4846 (multi-sensor design, cross-talk) were blocked and should be fetched on a machine that can reach st.com.

### H5 2S → 5 V buck (H5 page, power-flow SVG)

- 2S pack 7.4 V nominal, 8.4 V full → buck set to **5.00 V** before anything is connected → ESP32-C6 **5V pin**; sensors from the ESP32's 3V3 rail. Protected pack or BMS, fuse and main switch upstream.
- Motor VM comes from the motor power path, not this rail. Common ground everywhere.
- Never USB and the 5 V rail at the same time (H1/H5).
- **[analysis]** Bench debugging over USB with the battery connected therefore needs the buck output switched off, or the serial link moved to a separate 3.3 V USB-UART adapter on 16/17.

### H6 2× GA12-N20 6 V gearmotors with Hall encoders (H6 page, wiring SVG)

- 6 V motor, about 500 rpm no-load; two-channel Hall encoder (A, B); 4 GPIO inputs total.
- Encoder VCC from 3V3 **only if the encoder supports 3.3 V**; encoder signals above 3.6 V must never reach a GPIO. Wire colours and connector order vary: read the motor's label.
- A full 2S pack (8.4 V) exceeds the 6 V rating: "use a suitable motor supply or confirm an approved voltage limit before connecting VM".
- Ticks per revolution must be **measured** (gear ratio, encoder version and edge counting all change it). Distance per tick = π × wheel diameter / measured ticks per revolution.
- **[analysis]** The cheapest "approved limit" is a software duty cap of about 6/8.4 ≈ 70 % in the motion layer. Pulse counting on the ESP32 should use interrupts or the PCNT peripheral (H1), with encoder wires routed away from motor leads.

### C2 / C3 working practice

- C2: one shared sprint branch, small tested commits every 15–30 min, one owner per file, revert not reset on the shared branch.
- C3: keep a context pack under `docs/` (exact board and revision, verified pin map and wiring table, datasheet copies or links, library names and versions, build/flash/monitor commands, known-good results), ask agents for small changes with "do not commit or push", handoff notes with owner/files/tested/result/next test. `HANDOFF.md` already follows this; `docs/pinout.md` does not exist yet although README refers to it.

## 5. Findings against the current project (priority order)

- **F1 — serial-only speed ladder is illegal in the slot.** UKMARS forbids a console or dev device on the mouse during the contest. Level selection (`1`–`5`) and `f` must move to the mouse: e.g. short press = start, long press = cycle level, LED colour = level. Design change to the sketch's run control; solver untouched.
- **F2 — pin budget.** 14 usable header GPIOs versus 13 required plus button and LED. The placeholder button GPIO2 and LED GPIO7 are valid free pins but leave no room for an IMU INT or battery ADC. Team decision, options in §4 H1.
- **F3 — DRI0044 brakes at duty 0** and needs VCC = 3V3. The motion owner must know both before writing `forwardOneCell`.
- **F4 — IMU address may be 0x69** (solder jumper). Scan both; do not hard-code 0x68 alone.
- **F5 — motor voltage cap** (6 V motors on 8.4 V) belongs in the same single-source block as the pin map so nobody runs 100 % duty on day one.
- **F6 — 2 s stop at the start cell** before restarting is a rule, not a courtesy. The sketch already waits for a button press; keep it that way and do not add auto-restart.
- **F7 — bench-debug power conflict** (USB vs 5 V rail): document the procedure so the C6 is not fed from both.
- **F8 — README says the pin map is mirrored in `docs/pinout.md`;** it isn't. C3 asks for that file plus wiring table and datasheets in the repo.
- Confirmed unchanged: kit list, addresses 0x30/31/32, strapping list, 180 mm cell, cost constants, exploration strategy, goal geometry, start-cell walls.

## 6. Decisions needed

1. Commit this digest and `reference/` (4.5 MB, mostly the DevKitC-1 pinout PNG, the TB6612 and MPU-6050 PDFs) into the repo as the C3 context pack, or just the text parts?
2. Button/LED: on-board BOOT (GPIO9) and RGB LED (GPIO8), or external on two header pins?
3. Run control redesign for F1 (button-only interface): design it next, before any hardware code?
4. Motor supply: software duty cap, or will the team add a regulated motor rail? Changes what `forwardOneCell` assumes.
