// micromouse_esp32.ino — the solver on the ESP32-C6, with the hardware layer stubbed.
//
// Layers:
//   solver.h        the maze logic (tested offline; do not edit on the day)
//   HARDWARE LAYER  readWalls / forwardOneCell / turnLeft / turnRight / turnAround
//                   -> the motion and sensor owners fill these in
//   RUN CONTROL     start button, run ladder, serial console, dry-run mode
//
// Until HARDWARE_READY is 1, the sketch runs a DRY RUN against a built-in
// contest maze (UK 2016 final): no motors, no sensors, the solver just walks
// the map and prints every move to the serial monitor. That lets you flash
// this today and watch the logic work before a single wire is soldered.
//
// Serial console (115200):
//   g   go: start a run (same as the start button)
//   f   forget the maze (rules: a judge-requested recovery erases the map)
//   p   print the map as ASCII
//   s   status
//   1-5 set the cruise-speed level used for the NEXT speed run (the ladder)
//
// Board: ESP32-C6-DevKitC-1. 3.3 V logic only. See H1 in the resource hub.

#include "solver.h"
using namespace mm;

#define HARDWARE_READY 0     // set to 1 once the hardware layer below is real

// ============================================================ TEAM PIN MAP
// Keep this the single source of truth; docs/pinout.md must match.
// Avoid strapping pins GPIO4,5,8,9,15 and native USB GPIO12,13 (H1).
static const int PIN_START_BUTTON = 2;    // to GND, INPUT_PULLUP
static const int PIN_LED          = 7;    // status LED
// Motor driver (DRI0044): DIR1/PWM1 left, DIR2/PWM2 right   -> TODO
// Encoders: A/B per wheel                                    -> TODO
// I2C: SDA/SCL shared by 3x VL53L0X + SEN0142; XSHUT x3      -> TODO

// ============================================================ HARDWARE LAYER
// Contract: every function ends with the mouse stationary at a cell centre.
// speedLevel 1..5 is the ladder: 1 = search crawl, 5 = flat out.

#if HARDWARE_READY

bool readWalls(bool& left, bool& front, bool& right) {
  // TODO (sensor owner): read the three VL53L0X in a fixed order, reject
  // timeouts, apply hysteresis (two thresholds), sample at the cell centre.
  // Return false if a reading is invalid; the run control will stop the mouse.
  left = front = right = false;
  return false;
}
void forwardOneCell(int speedLevel) {
  // TODO (motion owner): 180 mm by encoder count, trapezoid profile, wall
  // centring PID on the side sensors, hold heading if a side wall vanishes,
  // reset longitudinal error on a front wall.
}
void turnLeft(int speedLevel)   { /* TODO: pivot -90° by gyro + encoders. Arc turn later. */ }
void turnRight(int speedLevel)  { /* TODO: pivot +90°. */ }
void turnAround(int speedLevel) { /* TODO: pivot 180°, ideally re-square on the back wall. */ }
void calibrateGyro()            { /* TODO: mouse absolutely still, average zero-rate offset. */ }

#else  // ---------------------------------------------------- DRY RUN STUBS

// UK 2016 final maze, walls per cell as N=1 E=2 S=4 W=8, index x + 16*y.
static const uint8_t DRY_MAZE[256] PROGMEM = {
0xE,0xC,0x5,0x5,0x4,0x5,0x5,0x5,0x4,0x5,0x5,0x4,0x5,0x5,0x5,0x6,0x8,0x3,0xC,0x6,0x9,0x5,0x5,0x5,0x3,0xC,0x6,0x9,0x5,0x5,0x4,0x3,0xA,0xC,0x3,0x8,0x4,0x5,0x4,0x5,0x5,0x3,0x9,0x5,0x6,0xE,0x8,0x6,0xA,0x9,0x6,0xA,0x8,0x4,0x1,0x5,0x4,0x5,0x5,0x6,0x8,0x2,0xA,0xA,0x8,0x6,0xA,0x8,0x3,0x9,0x4,0x5,0x1,0x5,0x6,0xA,0xA,0xA,0xA,0xA,0xA,0xA,0x8,0x1,0x5,0x5,0x3,0xC,0x5,0x5,0x1,0x2,0xA,0xA,0xA,0xA,0xA,0xA,0xA,0xC,0x5,0x5,0x6,0x9,0x5,0x5,0x6,0xA,0x8,0x2,0x8,0x3,0x8,0x3,0xA,0x9,0x5,0x5,0x2,0xC,0x4,0x6,0x8,0x2,0xA,0xA,0x9,0x6,0xA,0xC,0x3,0xC,0x4,0x6,0xA,0x9,0x3,0xA,0xA,0xA,0xA,0x9,0x6,0xA,0xA,0xA,0xC,0x3,0xA,0xA,0x8,0x5,0x6,0xA,0xA,0xA,0x8,0x6,0xA,0xA,0xA,0x9,0x3,0xC,0x3,0xA,0x8,0x5,0x1,0x1,0x0,0x3,0xA,0xA,0xA,0xA,0x8,0x6,0xC,0x2,0xC,0x2,0x9,0x5,0x5,0x5,0x3,0xC,0x2,0x8,0x2,0xA,0xA,0xA,0xA,0x9,0x2,0x9,0x5,0x4,0x5,0x5,0x5,0x2,0xA,0xA,0x8,0x2,0x9,0x2,0xA,0xC,0x1,0x5,0x5,0x1,0x5,0x4,0x5,0x2,0x9,0x2,0xA,0xA,0xC,0x3,0x9,0x1,0x5,0x5,0x4,0x5,0x5,0x1,0x6,0x9,0x5,0x1,0x1,0x2,0x9,0x5,0x5,0x5,0x5,0x5,0x1,0x5,0x5,0x5,0x1,0x5,0x5,0x5,0x5,0x3 };

static Mouse* dryMouse = 0;  // the stubs need the pose to fake sensor readings
static bool dryWall(uint8_t c, Dir d) { return (pgm_read_byte(&DRY_MAZE[c]) >> d) & 1; }

bool readWalls(bool& left, bool& front, bool& right) {
  uint8_t c = dryMouse->cell; Dir h = dryMouse->heading;
  left = dryWall(c, leftOf(h)); front = dryWall(c, h); right = dryWall(c, rightOf(h));
  return true;
}
void forwardOneCell(int)  { delay(30); }
void turnLeft(int)        { delay(20); }
void turnRight(int)       { delay(20); }
void turnAround(int)      { delay(40); }
void calibrateGyro()      {}

#endif

// ============================================================ RUN CONTROL
static Mouse mouse;
static int   speedLevel = 1;       // ladder for speed runs; search always uses 1
static int   runNumber  = 0;

static void printStatus() {
  Serial.printf("run=%d phase=%d cell=(%d,%d) heading=%c exploreLoops=%d speedRunReady=%d nextSpeedLevel=%d fastPath=%d moves\n",
                runNumber, (int)mouse.phase, cellX(mouse.cell), cellY(mouse.cell), dirChar(mouse.heading),
                mouse.exploreLoops, (int)mouse.speedRunReady, speedLevel, (int)mouse.fastPath.length);
}

// ASCII dump of the map as the mouse currently believes it. Unknown sides are
// drawn as '?' so phantom walls and gaps are obvious on the serial monitor.
static void printMap() {
  const Maze& m = mouse.maze;
  for (int y = SIZE - 1; y >= 0; --y) {
    for (int x = 0; x < SIZE; ++x) {
      uint8_t c = cellIndex(x, y);
      Serial.print('o'); Serial.print(!m.isKnown(c, N) ? " ? " : m.hasWall(c, N) ? "---" : "   ");
    }
    Serial.println('o');
    for (int x = 0; x < SIZE; ++x) {
      uint8_t c = cellIndex(x, y);
      Serial.print(!m.isKnown(c, W) ? '?' : m.hasWall(c, W) ? '|' : ' ');
      char mark = (c == mouse.cell) ? 'M' : m.isGoal(c) && m.goalCount == 4 ? 'G' : (x == 0 && y == 0) ? 'S' : ' ';
      Serial.print(' '); Serial.print(mark); Serial.print(' ');
    }
    Serial.println('|');
  }
  for (int x = 0; x < SIZE; ++x) Serial.print("o---");
  Serial.println('o');
}

static void printPath() {
  SegmentPath sp; compressPath(mouse.fastPath, sp);
  Serial.print("fast route: ");
  for (uint16_t i = 0; i < sp.length; ++i) {
    if (sp.segs[i].kind == 'S') { Serial.print('S'); Serial.print(sp.segs[i].count); Serial.print(' '); }
    else { Serial.print(sp.segs[i].kind); Serial.print(' '); }
  }
  Serial.println();
}

// Execute one run: search or speed, until the solver says STOP.
static void doRun() {
  runNumber++;
  mouse.startRun();
  bool speedRun = (mouse.phase == PHASE_SPEED_RUN);
  int level = speedRun ? speedLevel : 1;
  Serial.printf("\n=== run %d: %s (speed level %d)\n", runNumber, speedRun ? "SPEED RUN" : "search", level);
  if (speedRun) printPath();
  digitalWrite(PIN_LED, HIGH);

  for (;;) {
    bool l, f, r;
    if (!readWalls(l, f, r)) { Serial.println("sensor read failed - stopping"); break; }
    mouse.observe(l, f, r);
    Move m = mouse.next();
    if (m == MOVE_STOP) break;
    if (!mouse.moveIsSafe(m)) { Serial.println("BUG: unsafe move proposed - stopping"); break; }
    Serial.printf("(%2d,%2d) %c walls L%d F%d R%d -> %c\n", cellX(mouse.cell), cellY(mouse.cell), dirChar(mouse.heading), l, f, r, moveChar(m));
    switch (m) {
      case MOVE_LEFT:  turnLeft(level);   break;
      case MOVE_RIGHT: turnRight(level);  break;
      case MOVE_BACK:  turnAround(level); break;
      default: break;
    }
    forwardOneCell(level);
    mouse.applyMove(m);
  }
  digitalWrite(PIN_LED, LOW);

  if (mouse.phase == PHASE_DONE)            Serial.println("=== reached the centre on the fast route. Next run: raise the speed level (1-5) if this one was clean.");
  else if (mouse.phase == PHASE_SPEED_RUN)  Serial.println("=== route proven and planned. Next run is the speed run: bank it at a safe level first.");
  else if (mouse.maze.isGoal(mouse.cell))   Serial.println("=== reached the centre (search). Place the mouse back at the start for the return leg.");
  else                                      Serial.println("=== search loop complete, back at start; route not yet proven. Next run explores more.");
  printStatus();
}

void setup() {
  Serial.begin(115200);
  pinMode(PIN_START_BUTTON, INPUT_PULLUP);
  pinMode(PIN_LED, OUTPUT);
  mouse.begin();
#if !HARDWARE_READY
  dryMouse = &mouse;
  Serial.println("DRY RUN MODE: HARDWARE_READY=0, walking the built-in UK 2016 maze. Send 'g' to start a run.");
#else
  calibrateGyro();
  Serial.println("Ready. Press the start button or send 'g'.");
#endif
}

void loop() {
  if (Serial.available()) {
    char ch = Serial.read();
    if (ch == 'g') doRun();
    else if (ch == 'f') { mouse.forgetMaze(); runNumber = 0; Serial.println("maze forgotten"); }
    else if (ch == 'p') printMap();
    else if (ch == 's') printStatus();
    else if (ch >= '1' && ch <= '5') { speedLevel = ch - '0'; Serial.printf("next speed run at level %d\n", speedLevel); }
  }
  if (digitalRead(PIN_START_BUTTON) == LOW) {
    delay(50);
    while (digitalRead(PIN_START_BUTTON) == LOW) {}
    delay(1500);   // hands clear before it moves
    doRun();
  }
}
