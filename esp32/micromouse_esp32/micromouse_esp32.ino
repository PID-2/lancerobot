// micromouse_esp32.ino — run control for the Micromouse on the ESP32-C6-DevKitC-1.
//
// Files in this folder (the Arduino IDE compiles them together):
//   config.h        pin map, limits, calibration constants   <- the only file with hardware numbers
//   solver.h        maze logic, tested offline on 522 mazes  <- do not edit on the day
//   hal.h           what the run control needs from the hardware
//   hal_dryrun.h    HARDWARE_READY 0: fake sensors on a built-in maze, no motors
//   hal_esp32c6.h   HARDWARE_READY 1: DRI0044 motors, GA12-N20 encoders, 3x VL53L0X, SEN0142 IMU
//   board_ui.h      the on-board BOOT button and RGB LED
//   this file       button + LED contest interface, run state machine, bench serial console
//
// Contest interface. UKMARS rules forbid a console or laptop on the mouse during
// the contest, so everything needed in the slot is one button and one LED:
//   short press           idle: start a run after a 1.5 s hands-clear countdown
//                         running: emergency stop (wheels brake, map kept)
//                         fault: clear the fault
//   hold 1.5 s, release   next speed level 1 -> 5 -> 1 (LED blinks the new level)
//   hold 5 s, release     forget the maze (a judge-requested recovery erases the map)
//
//   LED steady blue       idle, the next run is a search
//   LED steady green      idle, route proven, the next run is a speed run
//   LED steady violet     idle, the last speed run reached the centre
//   ...blinking off N times every 3 s   speed level N for the next speed run
//   LED yellow flashing   countdown before the wheels move (press to cancel)
//   LED white / cyan      running a search / a speed run
//   LED dim               resting: the rules want 2 s stationary in the start cell
//   LED red flashing      fault; 's' on the console says why
//
// Bench serial console, 115200 (never connected in the slot):
//   g go    x stop    f forget maze    p print map    s status    1-5 speed level    ? help
//   w wall sensors    e encoders    i imu    m motor test (wheels off the table)
//   c recalibrate gyro    b battery

#include "config.h"
#include "solver.h"
#include "hal.h"
#if HARDWARE_READY
  #include "hal_esp32c6.h"
#else
  #include "hal_dryrun.h"
#endif

using namespace mm;

// ============================================================ STATE
enum State : uint8_t { ST_IDLE, ST_COUNTDOWN, ST_RUNNING, ST_REST, ST_FAULT };
enum RunEnd : uint8_t { RUN_FINISHED, RUN_ABORTED, RUN_FAULT };

static Mouse    mouse;
static State    state = ST_IDLE;
static uint32_t stateSince = 0;
static int      speedLevel = 1;          // ladder for speed runs; search always uses 1
static int      runNumber = 0;
static RunEnd   lastRunEnd = RUN_FINISHED;
static const char* faultReason = "none";
static bool     abortLatched = false;    // set by the button during a run
static bool     ignoreButtonUntilRelease = false;

static const char* stateName(State s) {
  switch (s) {
    case ST_IDLE:      return "IDLE";
    case ST_COUNTDOWN: return "COUNTDOWN";
    case ST_RUNNING:   return "RUNNING";
    case ST_REST:      return "REST";
    default:           return "FAULT";
  }
}

static const char* nextRunName() {
  if (mouse.phase == PHASE_DONE || mouse.speedRunReady) return "speed run";
  return "search";
}

static void setState(State s) {
  state = s;
  stateSince = millis();
  Serial.print("state: "); Serial.print(stateName(s));
  if (s == ST_IDLE) { Serial.print(" (next: "); Serial.print(nextRunName()); Serial.print(")"); }
  Serial.println();
}

static void fault(const char* why) {
  faultReason = why;
  Serial.print("FAULT: "); Serial.println(why);
  setState(ST_FAULT);
}

// ============================================================ LED
static void ledColour(uint8_t r, uint8_t g, uint8_t b) {
  static uint8_t lr = 1, lg = 1, lb = 1;   // impossible start value so the first write happens
  if (r == lr && g == lg && b == lb) return;
  lr = r; lg = g; lb = b;
  hal::led(r, g, b);
}

static void idleColour(uint8_t& r, uint8_t& g, uint8_t& b) {
  if (mouse.phase == PHASE_DONE)  { r = 150; g = 0;   b = 255; }   // violet: centre reached on the fast route
  else if (mouse.speedRunReady)   { r = 0;   g = 255; b = 0;   }   // green: speed run next
  else                            { r = 0;   g = 40;  b = 255; }   // blue: search next
}

// Blocking blink, used only when idle to acknowledge a button gesture.
static void blink(uint8_t r, uint8_t g, uint8_t b, int times) {
  for (int i = 0; i < times; ++i) {
    ledColour(r, g, b); delay(150);
    ledColour(0, 0, 0); delay(150);
  }
}

static void updateLed() {
  const uint32_t t = millis() - stateSince;
  uint8_t r = 0, g = 0, b = 0;
  switch (state) {
    case ST_IDLE: {
      idleColour(r, g, b);
      // Blink off speedLevel times at the end of every period.
      const uint32_t phase = t % IDLE_BLINK_PERIOD_MS;
      const uint32_t blinkStart = IDLE_BLINK_PERIOD_MS - (uint32_t)speedLevel * 300;
      if (phase >= blinkStart && ((phase - blinkStart) % 300) < 120) { r = 0; g = 0; b = 0; }
      break;
    }
    case ST_COUNTDOWN:
      if ((t / 100) % 2) { r = 255; g = 180; b = 0; } else { r = 0; g = 0; b = 0; }
      break;
    case ST_REST:
      idleColour(r, g, b); r /= 6; g /= 6; b /= 6;
      break;
    case ST_FAULT:
      if ((t / 250) % 2) { r = 255; g = 0; b = 0; } else { r = 0; g = 0; b = 0; }
      break;
    default:
      return;   // running: the colour was set when the run started
  }
  ledColour(r, g, b);
}

// ============================================================ REPORTING
static void printStatus() {
  Serial.printf("state=%s run=%d next=%s level=%d phase=%d cell=(%d,%d) heading=%c exploreLoops=%d speedRunReady=%d fastPath=%d moves fault=%s",
                stateName(state), runNumber, nextRunName(), speedLevel, (int)mouse.phase,
                cellX(mouse.cell), cellY(mouse.cell), dirChar(mouse.heading),
                mouse.exploreLoops, (int)mouse.speedRunReady, (int)mouse.fastPath.length, faultReason);
  const float v = hal::batteryVolts();
  if (v > 0.0f) Serial.printf(" battery=%.2fV", v);
  Serial.println();
}

// ASCII dump of the map as the mouse currently believes it. Unknown sides are
// drawn as '?' so phantom walls and gaps are obvious on the serial monitor.
static void printMap() {
  const Maze& m = mouse.maze;
  for (int y = SIZE - 1; y >= 0; --y) {
    for (int x = 0; x < SIZE; ++x) {
      const uint8_t c = cellIndex(x, y);
      Serial.print('o'); Serial.print(!m.isKnown(c, N) ? " ? " : m.hasWall(c, N) ? "---" : "   ");
    }
    Serial.println('o');
    for (int x = 0; x < SIZE; ++x) {
      const uint8_t c = cellIndex(x, y);
      Serial.print(!m.isKnown(c, W) ? '?' : m.hasWall(c, W) ? '|' : ' ');
      const char mark = (c == mouse.cell) ? 'M' : (m.isGoal(c) && m.goalCount == 4) ? 'G' : (x == 0 && y == 0) ? 'S' : ' ';
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
    if (sp.segs[i].kind == 'S') { Serial.print('S'); Serial.print((int)sp.segs[i].count); Serial.print(' '); }
    else { Serial.print(sp.segs[i].kind); Serial.print(' '); }
  }
  Serial.println();
}

static void printHelp() {
  Serial.println("g go   x stop   f forget maze   p map   s status   1-5 speed level   ? help");
  Serial.println("w walls   e encoders   i imu   m motor test (wheels off the table)   c gyro cal   b battery");
  Serial.println("button: short = go / stop / clear fault, hold 1.5 s = next level, hold 5 s = forget maze");
}

// ============================================================ THE RUN
// Blocking: drives one run until the solver says STOP, the button aborts, or
// something fails. Every hardware call polls runAbortRequested().
bool runAbortRequested() {
  if (abortLatched) return true;
  static uint32_t pressedSince = 0;
  if (hal::buttonPressed()) {
    if (pressedSince == 0) pressedSince = millis();
    else if (millis() - pressedSince >= 50) { abortLatched = true; return true; }
  } else {
    pressedSince = 0;
  }
  return false;
}

static RunEnd doRun() {
  runNumber++;
  mouse.startRun();
  const bool speedRun = (mouse.phase == PHASE_SPEED_RUN);
  const int level = speedRun ? speedLevel : 1;
  Serial.printf("\n=== run %d: %s (speed level %d)\n", runNumber, speedRun ? "SPEED RUN" : "search", level);
  if (speedRun) printPath();

  const char* why = hal::runBegin(level);
  if (why) { faultReason = why; return RUN_FAULT; }
  abortLatched = false;
  if (speedRun) ledColour(0, 255, 255); else ledColour(255, 255, 255);

  RunEnd end = RUN_FINISHED;
  for (;;) {
    if (runAbortRequested()) { end = RUN_ABORTED; break; }
    bool l, f, r;
    if (!hal::readWalls(l, f, r)) { faultReason = hal::lastError(); end = RUN_FAULT; break; }
    mouse.observe(l, f, r);
    const Move m = mouse.next();
    if (m == MOVE_STOP) break;
    if (!mouse.moveIsSafe(m)) {
      faultReason = "solver proposed a move through a wall: press p, a phantom wall is a sensor problem";
      end = RUN_FAULT; break;
    }
    Serial.printf("(%2d,%2d) %c walls L%d F%d R%d -> %c\n", cellX(mouse.cell), cellY(mouse.cell), dirChar(mouse.heading), (int)l, (int)f, (int)r, moveChar(m));
    bool ok = true;
    switch (m) {
      case MOVE_LEFT:  ok = hal::turnLeft(level);   break;
      case MOVE_RIGHT: ok = hal::turnRight(level);  break;
      case MOVE_BACK:  ok = hal::turnAround(level); break;
      default: break;
    }
    if (ok) ok = hal::forwardOneCell(level);
    if (!ok) {
      if (runAbortRequested()) end = RUN_ABORTED;
      else { faultReason = hal::lastError(); end = RUN_FAULT; }
      break;
    }
    mouse.applyMove(m);
  }
  hal::runEnd();

  if (end == RUN_ABORTED)     Serial.println("=== STOPPED by the button. Map kept. Put the mouse back in the start cell.");
  else if (end == RUN_FAULT)  Serial.println("=== run ended on a fault.");
  else if (mouse.phase == PHASE_DONE)           Serial.println("=== reached the centre on the fast route. Bank it, then raise the level (hold the button) if it was clean.");
  else if (mouse.phase == PHASE_SPEED_RUN)      Serial.println("=== route proven and planned. Next run is the speed run: bank it at a safe level first.");
  else if (mouse.maze.isGoal(mouse.cell))       Serial.println("=== stopped at the centre. Put the mouse back in the start cell.");
  else                                          Serial.println("=== search loop complete, back at start; route not yet proven. Next run explores more.");
  printStatus();
  return end;
}

// ============================================================ BUTTON GESTURES
static void onShortPress() {
  switch (state) {
    case ST_IDLE:
      Serial.println("button: go");
      setState(ST_COUNTDOWN);
      break;
    case ST_COUNTDOWN:
      Serial.println("button: countdown cancelled");
      setState(ST_IDLE);
      break;
    case ST_FAULT:
      Serial.println("button: fault cleared");
      setState(ST_IDLE);
      break;
    default:
      break;   // resting: the rules want the mouse still for 2 s
  }
}

static void cycleLevel() {
  speedLevel = (speedLevel % 5) + 1;
  Serial.printf("speed level for the next speed run: %d\n", speedLevel);
  if (state == ST_IDLE || state == ST_REST) blink(255, 255, 255, speedLevel);
}

static void forgetMaze() {
  mouse.forgetMaze();
  runNumber = 0;
  Serial.println("maze forgotten (map erased, search starts from nothing)");
  if (state == ST_IDLE || state == ST_REST || state == ST_FAULT) { blink(255, 0, 0, 3); setState(ST_IDLE); }
}

// Debounced press/hold/release decoder. Runs only between runs; during a run
// the hardware layer polls runAbortRequested() instead.
static void pollButton() {
  static bool     stable = false;       // debounced state, true = pressed
  static bool     raw = false;
  static uint32_t rawSince = 0;
  static uint32_t pressedAt = 0;
  static bool     longShown = false, forgetShown = false;

  const uint32_t now = millis();
  const bool r = hal::buttonPressed();
  if (r != raw) { raw = r; rawSince = now; }
  if (now - rawSince < BUTTON_DEBOUNCE_MS) return;

  if (raw && !stable) {                 // press
    stable = true; pressedAt = now; longShown = forgetShown = false;
  } else if (raw && stable) {           // held: preview what a release would do
    const uint32_t held = now - pressedAt;
    if (held >= BUTTON_FORGET_MS && !forgetShown) { forgetShown = true; ledColour(255, 0, 0); }
    else if (held >= BUTTON_LONG_MS && !longShown) { longShown = true; ledColour(255, 255, 255); }
  } else if (!raw && stable) {          // release
    stable = false;
    const uint32_t held = now - pressedAt;
    if (ignoreButtonUntilRelease) { ignoreButtonUntilRelease = false; return; }
    if (held >= BUTTON_FORGET_MS) forgetMaze();
    else if (held >= BUTTON_LONG_MS) cycleLevel();
    else onShortPress();
  }
}

// ============================================================ SERIAL CONSOLE
static void pollSerial() {
  while (Serial.available()) {
    const char ch = (char)Serial.read();
    switch (ch) {
      case 'g': if (state == ST_IDLE || state == ST_FAULT) setState(ST_COUNTDOWN); else Serial.println("not idle"); break;
      case 'x': if (state == ST_COUNTDOWN) setState(ST_IDLE); else Serial.println("nothing to stop (during a run, press the button)"); break;
      case 'f': forgetMaze(); break;
      case 'p': printMap(); break;
      case 's': printStatus(); break;
      case '?': printHelp(); break;
      case 'w': hal::benchWalls(); break;
      case 'e': hal::benchEncoders(); break;
      case 'i': hal::benchImu(); break;
      case 'm': hal::benchMotors(); break;
      case 'c': hal::calibrateGyro(); break;
      case 'b': hal::benchBattery(); break;
      default:
        if (ch >= '1' && ch <= '5') { speedLevel = ch - '0'; Serial.printf("speed level for the next speed run: %d\n", speedLevel); }
        break;
    }
  }
}

// ============================================================ ARDUINO
void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println();
  Serial.println("Micromouse ESP32-C6 " HARDWARE_READY_NAME);
  mouse.begin();
  hal::dryRunAttach(&mouse);
  const char* why = hal::begin();
  if (why) fault(why);
  else {
    Serial.println("hardware ready. Short press or 'g' starts a run; '?' lists the console commands.");
    setState(ST_IDLE);
  }
}

void loop() {
  pollButton();
  pollSerial();
  updateLed();

  switch (state) {
    case ST_COUNTDOWN:
      if (millis() - stateSince >= START_DELAY_MS) {
        setState(ST_RUNNING);
        lastRunEnd = doRun();
        if (lastRunEnd == RUN_ABORTED) ignoreButtonUntilRelease = hal::buttonPressed();
        if (lastRunEnd == RUN_FAULT) fault(faultReason); else setState(ST_REST);
      }
      break;
    case ST_REST:
      if (millis() - stateSince >= REST_AFTER_RUN_MS) setState(ST_IDLE);
      break;
    default:
      break;
  }
  delay(2);
}
