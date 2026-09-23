// hal.h — what the run control needs from the hardware, and nothing else.
//
// Two implementations, chosen by HARDWARE_READY in config.h:
//   hal_dryrun.h   fake sensors walking a built-in maze, no motors (flash on day one)
//   hal_esp32c6.h  the real mouse: DRI0044, GA12-N20 encoders, 3x VL53L0X, SEN0142
//
// Contract: every motion primitive starts and ends with the mouse stationary at
// a cell centre, and polls runAbortRequested() often enough that a button press
// stops the wheels within about 50 ms.

#pragma once
#include <stdint.h>

namespace mm { struct Mouse; }

namespace hal {

// Power-up initialisation, called once from setup(). Returns nullptr when every
// subsystem answered, otherwise a static string naming the first thing that
// failed. Whatever failed, the LED and button keep working so the fault shows.
const char* begin();

// Why the last call that returned false failed. Static string, never nullptr.
const char* lastError();

// Bracket a run. runBegin() checks that the hardware and calibration are fit
// to drive and returns nullptr, or a static string saying what is missing.
const char* runBegin(int level);
void runEnd();

// Wall readings taken at the current cell centre, after hysteresis. Returns
// false if the readings are not trustworthy; the run control then stops.
bool readWalls(bool& left, bool& front, bool& right);

// Motion primitives, level 1..5 from the speed ladder in config.h.
// L/R turn 90 degrees on the spot, B turns 180; the run control then calls
// forwardOneCell() for the cell that follows (solver move semantics).
// Return false when aborted (runAbortRequested() became true) or failed
// (see lastError()); the wheels are braked either way.
bool forwardOneCell(int level);
bool turnLeft(int level);
bool turnRight(int level);
bool turnAround(int level);

// On-board user interface.
void led(uint8_t r, uint8_t g, uint8_t b);   // 0..255 each, brightness applied here
bool buttonPressed();                         // true while the BOOT button is held

// Bench diagnostics for the serial console. Each prints what it measured.
void benchWalls();      // stream the three distances and the wall decisions
void benchEncoders();   // stream the counts while a wheel is turned by hand
void benchImu();        // stream yaw rate and integrated yaw
void benchMotors();     // wheels off the table: each motor forward then reverse at low power
void benchBattery();    // battery volts, if a monitor is fitted
void calibrateGyro();   // re-measure the gyro zero-rate bias, mouse still
float batteryVolts();   // 0 when no monitor is fitted

// Dry run only: where the fake sensors take the mouse's position from.
void dryRunAttach(const mm::Mouse* m);

} // namespace hal

// Provided by the run control. The hardware layer polls it inside every move.
bool runAbortRequested();
