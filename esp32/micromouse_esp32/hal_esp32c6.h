// hal_esp32c6.h — HARDWARE_READY 1: the real mouse.
//
//   DRI0044 (TB6612FNG)   one DIR + one PWM per wheel; STBY is tied high on the
//                         module and DIR feeds both H-bridge inputs through an
//                         inverter, so PWM 0 is a SHORT BRAKE, never a coast
//                         (docs/reference/datasheets/DRI0044_schematic.pdf, TB6612 table)
//   GA12-N20 encoders     A/B quadrature on four interrupt pins, 4 edges counted
//   3x VL53L0X (GY-530)   shared I2C, XSHUT each, re-addressed 0x30/0x31/0x32 at every boot (H4)
//   SEN0142 (MPU-6050)    I2C 0x68, or 0x69 if the AD0 solder jumper is open (schematic)
//
// CALIBRATION CHECKLIST — do these on the bench, in this order, before a run
// (config.h holds every number; H1 §7 has the safe bring-up order):
//   1. 'm' motor test, wheels off the table: each wheel must run FORWARD first.
//      Wrong way -> flip MOTOR_x_INVERT.                                     (H2)
//   2. 'e' encoders: roll each wheel forward by hand, its count must go UP.
//      Wrong way -> flip ENC_x_SIGN. One full turn -> ENCODER_TICKS_PER_REV. (H6)
//   3. Measure WHEEL_DIAMETER_MM and TRACK_WIDTH_MM with calipers.
//   4. 'i' imu: turn the mouse LEFT by hand, yaw must go POSITIVE.
//      Wrong sign -> flip GYRO_YAW_SIGN.                                     (H3)
//   5. 'w' walls on the real maze under the real lights: set the four wall
//      thresholds, SIDE_CENTRED_MM and FRONT_STOP_MM.                       (S4, H4)
//   6. Drive one cell ten times and one 90 degree turn ten times, then tune
//      the gains and the speed ladder.                                       (S5 hour 1-2)
//
// Nothing in this file has run on the real mouse yet (written 2026-09-23 from
// the datasheets); expect gain tuning, not restructuring.

#pragma once
#include <Arduino.h>
#include <Wire.h>
#include <VL53L0X.h>    // Pololu, 1.3.1
#include <MPU6050.h>    // Electronic Cats (I2Cdev + MPU6050 by Jeff Rowberg), 1.4.5
#include <math.h>
#include "config.h"
#include "solver.h"
#include "hal.h"
#include "board_ui.h"

#if __has_include("hal/gpio_ll.h")
  #include "hal/gpio_ll.h"     // inline register read, safe inside an ISR
  #define ENC_READ(pin) gpio_ll_get_level(&GPIO, (uint32_t)(pin))
#else
  #define ENC_READ(pin) digitalRead(pin)
#endif

namespace hal {

// ============================================================ STATE
enum Side { LEFT = 0, FRONT = 1, RIGHT = 2 };
static const char* SIDE_NAME[3] = { "left", "front", "right" };
static const uint8_t TOF_ADDR[3] = { I2C_ADDR_TOF_LEFT, I2C_ADDR_TOF_FRONT, I2C_ADDR_TOF_RIGHT };
static const int     TOF_XSHUT[3] = { PIN_XSHUT_LEFT, PIN_XSHUT_FRONT, PIN_XSHUT_RIGHT };

static const char* errorText = "no error";
static bool  hardwareOk = false;
static float dutyCap = MOTOR_DUTY_CAP;

static VL53L0X  tof[3];
static uint16_t tofMm[3] = { 0, 0, 0 };
static uint32_t tofAt[3] = { 0, 0, 0 };
static bool     wallState[3] = { false, false, false };

static MPU6050  imuLow(I2C_ADDR_IMU_AD0_LOW, &Wire);
static MPU6050  imuHigh(I2C_ADDR_IMU_AD0_HIGH, &Wire);
static MPU6050* imu = nullptr;
static uint8_t  imuAddr = 0;
static float    gyroBiasZ = 0.0f;               // raw LSB
static const float GYRO_LSB_PER_DPS = 65.5f;    // MPU-6050 at +-500 deg/s full scale
static float    yawDeg = 0.0f;                  // integrated since the current move began, left positive

static volatile int32_t encCount[2] = { 0, 0 };
static volatile uint8_t encPrev[2] = { 0, 0 };
static const int8_t DRAM_ATTR QUAD[16] = { 0, +1, -1, 0,  -1, 0, 0, +1,  +1, 0, 0, -1,  0, -1, +1, 0 };   // in RAM: read from an ISR

static const uint32_t PWM_MAX = (1u << MOTOR_PWM_BITS) - 1;
static const float DEG2RAD = 0.017453292f;
static const float RAD2DEG = 57.29578f;

static float mmPerTick() {
  if (ENCODER_TICKS_PER_REV <= 0.0f || WHEEL_DIAMETER_MM <= 0.0f) return 0.0f;
  return (float)M_PI * WHEEL_DIAMETER_MM / ENCODER_TICKS_PER_REV;
}

// ============================================================ USER INTERFACE
void led(uint8_t r, uint8_t g, uint8_t b) { boardLedWrite(r, g, b); }
bool buttonPressed() { return boardButtonPressed(); }

// ============================================================ MOTORS
static void motorsInit() {
  pinMode(PIN_MOTOR_L_DIR, OUTPUT);
  pinMode(PIN_MOTOR_R_DIR, OUTPUT);
  digitalWrite(PIN_MOTOR_L_DIR, LOW);
  digitalWrite(PIN_MOTOR_R_DIR, LOW);
  ledcAttach(PIN_MOTOR_L_PWM, MOTOR_PWM_HZ, MOTOR_PWM_BITS);
  ledcAttach(PIN_MOTOR_R_PWM, MOTOR_PWM_HZ, MOTOR_PWM_BITS);
  ledcWrite(PIN_MOTOR_L_PWM, 0);
  ledcWrite(PIN_MOTOR_R_PWM, 0);
}

// cmd is -1..+1 as a fraction of the motor's RATED voltage; the duty cap turns
// that into a fraction of the battery voltage so 6 V motors never see more.
static void motorSet(bool rightWheel, float cmd) {
  if (cmd > 1.0f) cmd = 1.0f;
  if (cmd < -1.0f) cmd = -1.0f;
  const bool forward = cmd >= 0.0f;
  const bool invert = rightWheel ? MOTOR_RIGHT_INVERT : MOTOR_LEFT_INVERT;
  digitalWrite(rightWheel ? PIN_MOTOR_R_DIR : PIN_MOTOR_L_DIR, (forward != invert) ? HIGH : LOW);
  const float duty = fabsf(cmd) * dutyCap;
  ledcWrite(rightWheel ? PIN_MOTOR_R_PWM : PIN_MOTOR_L_PWM, (uint32_t)(duty * PWM_MAX + 0.5f));
}

static void motorsBrake() {          // TB6612: PWM low with DIR either way = short brake
  ledcWrite(PIN_MOTOR_L_PWM, 0);
  ledcWrite(PIN_MOTOR_R_PWM, 0);
}

// ============================================================ ENCODERS
static void IRAM_ATTR encUpdate(int w, int pinA, int pinB) {
  const uint8_t s = (uint8_t)((ENC_READ(pinA) << 1) | ENC_READ(pinB));
  encCount[w] += QUAD[(encPrev[w] << 2) | s];
  encPrev[w] = s;
}
static void IRAM_ATTR encLeftIsr()  { encUpdate(0, PIN_ENC_L_A, PIN_ENC_L_B); }
static void IRAM_ATTR encRightIsr() { encUpdate(1, PIN_ENC_R_A, PIN_ENC_R_B); }

static void encodersInit() {
  pinMode(PIN_ENC_L_A, INPUT_PULLUP); pinMode(PIN_ENC_L_B, INPUT_PULLUP);
  pinMode(PIN_ENC_R_A, INPUT_PULLUP); pinMode(PIN_ENC_R_B, INPUT_PULLUP);
  encPrev[0] = (uint8_t)((digitalRead(PIN_ENC_L_A) << 1) | digitalRead(PIN_ENC_L_B));
  encPrev[1] = (uint8_t)((digitalRead(PIN_ENC_R_A) << 1) | digitalRead(PIN_ENC_R_B));
  attachInterrupt(digitalPinToInterrupt(PIN_ENC_L_A), encLeftIsr, CHANGE);
  attachInterrupt(digitalPinToInterrupt(PIN_ENC_L_B), encLeftIsr, CHANGE);
  attachInterrupt(digitalPinToInterrupt(PIN_ENC_R_A), encRightIsr, CHANGE);
  attachInterrupt(digitalPinToInterrupt(PIN_ENC_R_B), encRightIsr, CHANGE);
}

static int32_t encLeft()  { noInterrupts(); const int32_t c = encCount[0]; interrupts(); return c * ENC_LEFT_SIGN; }
static int32_t encRight() { noInterrupts(); const int32_t c = encCount[1]; interrupts(); return c * ENC_RIGHT_SIGN; }

// ============================================================ DISTANCE SENSORS
static void i2cScan() {
  Serial.print("I2C scan:");
  int found = 0;
  for (uint8_t a = 1; a < 127; ++a) {
    Wire.beginTransmission(a);
    if (Wire.endTransmission() == 0) { Serial.printf(" 0x%02X", a); ++found; }
  }
  if (!found) Serial.print(" nothing");
  Serial.println();
}

// H4: all XSHUT low, then wake one sensor at a time, initialise it at 0x29 and
// move it to its own address. New addresses are lost at reset, so every boot.
static const char* tofBoot() {
  for (int i = 0; i < 3; ++i) { pinMode(TOF_XSHUT[i], OUTPUT); digitalWrite(TOF_XSHUT[i], LOW); }
  delay(10);
  const char* firstFailure = nullptr;
  for (int i = 0; i < 3; ++i) {
    digitalWrite(TOF_XSHUT[i], HIGH);
    delay(10);
    tof[i].setBus(&Wire);
    tof[i].setTimeout(50);
    if (!tof[i].init()) {
      Serial.printf("%s VL53L0X init: FAILED (no answer at 0x%02X; check VIN, GND, SDA, SCL and that XSHUT is wired to GPIO%d)\n",
                    SIDE_NAME[i], I2C_ADDR_TOF_BOOT, TOF_XSHUT[i]);
      if (!firstFailure) firstFailure = (i == LEFT) ? "left VL53L0X init failed" : (i == FRONT) ? "front VL53L0X init failed" : "right VL53L0X init failed";
      digitalWrite(TOF_XSHUT[i], LOW);   // keep it off the bus so the next sensor can use 0x29
      continue;
    }
    tof[i].setAddress(TOF_ADDR[i]);
    tof[i].setMeasurementTimingBudget(TOF_TIMING_BUDGET_US);
    tof[i].startContinuous();
    Serial.printf("%s VL53L0X init: OK, new address 0x%02X\n", SIDE_NAME[i], TOF_ADDR[i]);
  }
  return firstFailure;
}

// Non-blocking: collect whichever sensors have a fresh measurement ready.
static void tofService() {
  for (int i = 0; i < 3; ++i) {
    if ((tof[i].readReg(VL53L0X::RESULT_INTERRUPT_STATUS) & 0x07) == 0) continue;
    const uint16_t mm = tof[i].readRangeContinuousMillimeters();
    if (tof[i].timeoutOccurred()) continue;
    tofMm[i] = mm;
    tofAt[i] = millis();
  }
}

static bool tofFresh(int i) { return tofAt[i] != 0 && (millis() - tofAt[i]) < TOF_STALE_MS; }

// Two thresholds (S4): a wall appears below ON and disappears above OFF.
static bool decideWall(int i, uint16_t onMm, uint16_t offMm) {
  uint16_t mm = tofMm[i];
  if (mm > TOF_VALID_MAX_MM) mm = TOF_VALID_MAX_MM;   // 8190 and friends mean "nothing in range"
  if (mm < onMm) wallState[i] = true;
  else if (mm > offMm) wallState[i] = false;
  return wallState[i];
}

bool readWalls(bool& left, bool& front, bool& right) {
  const uint32_t t0 = millis();
  while (!(tofFresh(LEFT) && tofFresh(FRONT) && tofFresh(RIGHT))) {
    tofService();
    if (millis() - t0 > TOF_STALE_MS) {
      errorText = !tofFresh(LEFT) ? "left VL53L0X stopped answering" : !tofFresh(FRONT) ? "front VL53L0X stopped answering" : "right VL53L0X stopped answering";
      return false;
    }
    delay(1);
  }
  left  = decideWall(LEFT,  WALL_SIDE_ON_MM,  WALL_SIDE_OFF_MM);
  front = decideWall(FRONT, WALL_FRONT_ON_MM, WALL_FRONT_OFF_MM);
  right = decideWall(RIGHT, WALL_SIDE_ON_MM,  WALL_SIDE_OFF_MM);
  return true;
}

// ============================================================ IMU
static const char* imuBoot() {
  imu = nullptr;
  const uint8_t candidates[2] = { I2C_ADDR_IMU_AD0_LOW, I2C_ADDR_IMU_AD0_HIGH };
  for (int i = 0; i < 2; ++i) {
    Wire.beginTransmission(candidates[i]);
    if (Wire.endTransmission() == 0) { imuAddr = candidates[i]; imu = (i == 0) ? &imuLow : &imuHigh; break; }
  }
  if (!imu) {
    Serial.println("IMU: nothing answers at 0x68 or 0x69 (check VIN, GND, SDA, SCL)");
    return "IMU not found at 0x68 or 0x69";
  }
  imu->initialize();
  if (!imu->testConnection()) {
    Serial.printf("IMU at 0x%02X: WHO_AM_I mismatch, not an MPU-6050?\n", imuAddr);
    return "IMU answered but is not an MPU-6050";
  }
  imu->setFullScaleGyroRange(MPU6050_GYRO_FS_500);
  imu->setDLPFMode(MPU6050_DLPF_BW_42);
  Serial.printf("IMU init: OK at 0x%02X (MPU-6050, gyro +-500 deg/s)\n", imuAddr);
  return nullptr;
}

static float yawRateDps() {
  if (!imu) return 0.0f;
  return GYRO_YAW_SIGN * ((float)imu->getRotationZ() - gyroBiasZ) / GYRO_LSB_PER_DPS;
}

void calibrateGyro() {
  if (!imu) { Serial.println("gyro calibration: no IMU"); return; }
  Serial.println("gyro calibration: keep the mouse absolutely still...");
  int32_t sum = 0; int16_t lo = 32767, hi = -32768;
  for (uint16_t i = 0; i < GYRO_CAL_SAMPLES; ++i) {
    const int16_t z = imu->getRotationZ();
    sum += z; if (z < lo) lo = z; if (z > hi) hi = z;
    delay(2);
  }
  gyroBiasZ = (float)sum / GYRO_CAL_SAMPLES;
  Serial.printf("gyro bias z = %.1f LSB (spread %d LSB)%s\n", gyroBiasZ, (int)(hi - lo),
                (hi - lo) > 200 ? "  WARNING: the mouse was moving, calibrate again" : "");
}

// ============================================================ MOTION
struct WheelLoop { int32_t lastTicks; float integral; };
static WheelLoop wheelLoop[2];

static void wheelsReset() {
  wheelLoop[0].lastTicks = encLeft();  wheelLoop[0].integral = 0.0f;
  wheelLoop[1].lastTicks = encRight(); wheelLoop[1].integral = 0.0f;
}

// Per-wheel speed loop: feed-forward plus PI on the encoder speed.
static void wheelsDrive(float vLeft, float vRight, float dt) {
  const float k = mmPerTick();
  const float targets[2] = { vLeft, vRight };
  for (int w = 0; w < 2; ++w) {
    const int32_t ticks = (w == 0) ? encLeft() : encRight();
    const float measured = (float)(ticks - wheelLoop[w].lastTicks) * k / dt;
    wheelLoop[w].lastTicks = ticks;
    const float err = targets[w] - measured;
    wheelLoop[w].integral += err * dt;
    if (wheelLoop[w].integral > 200.0f) wheelLoop[w].integral = 200.0f;
    if (wheelLoop[w].integral < -200.0f) wheelLoop[w].integral = -200.0f;
    const float cmd = WHEEL_KFF * targets[w] + WHEEL_KP * err + WHEEL_KI * wheelLoop[w].integral;
    motorSet(w == 1, cmd);
  }
}

// Wait for the next control tick; returns the real dt in seconds.
static float controlTick(uint32_t& lastUs) {
  uint32_t now = micros();
  while (now - lastUs < CONTROL_PERIOD_US) { delayMicroseconds(100); now = micros(); }
  const float dt = (float)(now - lastUs) * 1e-6f;
  lastUs = now;
  return dt;
}

static bool sideWall(int i) { return tofFresh(i) && tofMm[i] < WALL_SIDE_OFF_MM; }

// Drive exactly one cell (S4): trapezoid on encoder distance, side-wall
// centring while walls exist, gyro heading hold when they vanish, and the
// front wall as a free reset of longitudinal error at the end.
bool forwardOneCell(int level) {
  if (level < 1) level = 1;
  if (level > 5) level = 5;
  const SpeedLevel& L = SPEED_LEVELS[level - 1];
  const float k = mmPerTick();
  const int32_t l0 = encLeft(), r0 = encRight();
  wheelsReset();
  yawDeg = 0.0f;
  float v = 0.0f;
  const uint32_t t0 = millis();
  uint32_t lastUs = micros();

  for (;;) {
    const float dt = controlTick(lastUs);
    tofService();
    yawDeg += yawRateDps() * dt;

    const float dist = 0.5f * (float)((encLeft() - l0) + (encRight() - r0)) * k;
    float remaining = CELL_MM - dist;
    if (tofFresh(FRONT) && tofMm[FRONT] < WALL_FRONT_ON_MM + 40) remaining = (float)tofMm[FRONT] - (float)FRONT_STOP_MM;
    if (remaining <= 0.0f) break;

    float vAllowed = sqrtf(2.0f * L.accel_mm_s2 * remaining);
    if (vAllowed < 40.0f) vAllowed = 40.0f;                 // always creep the last millimetres
    v += L.accel_mm_s2 * dt;
    if (v > L.cruise_mm_s) v = L.cruise_mm_s;
    if (v > vAllowed) v = vAllowed;

    float steer;                                            // mm/s of wheel-speed difference, positive = turn left
    const bool lw = sideWall(LEFT), rw = sideWall(RIGHT);
    if (lw && rw)      steer = WALL_KP * (float)((int)tofMm[LEFT] - (int)tofMm[RIGHT]);
    else if (lw)       steer = WALL_KP * 2.0f * (float)((int)tofMm[LEFT] - (int)SIDE_CENTRED_MM);
    else if (rw)       steer = -WALL_KP * 2.0f * (float)((int)tofMm[RIGHT] - (int)SIDE_CENTRED_MM);
    else               steer = -HEADING_KP * yawDeg;        // no walls: hold the heading we started with
    if (steer > 0.3f * v) steer = 0.3f * v;
    if (steer < -0.3f * v) steer = -0.3f * v;

    wheelsDrive(v - steer, v + steer, dt);

    if (runAbortRequested()) { motorsBrake(); return false; }
    if (millis() - t0 > MOVE_TIMEOUT_MS) { motorsBrake(); errorText = "forwardOneCell timed out (wheel stalled or encoder not counting)"; return false; }
  }
  motorsBrake();
  delay(SETTLE_MS);
  tofService();
  return true;
}

// Pivot on the spot by the gyro, positive = left, then cross-check the
// encoders and warn on the console if they disagree badly.
static bool pivot(float degrees, int level) {
  if (level < 1) level = 1;
  if (level > 5) level = 5;
  const SpeedLevel& L = SPEED_LEVELS[level - 1];
  const float dir = degrees >= 0.0f ? 1.0f : -1.0f;
  const float target = fabsf(degrees);
  const int32_t l0 = encLeft(), r0 = encRight();
  wheelsReset();
  yawDeg = 0.0f;
  float w = 0.0f;                                            // deg/s
  const uint32_t t0 = millis();
  uint32_t lastUs = micros();

  for (;;) {
    const float dt = controlTick(lastUs);
    yawDeg += yawRateDps() * dt;
    const float remaining = target - dir * yawDeg;
    if (remaining <= TURN_DONE_DEG) break;

    float wCmd = TURN_KP * remaining;
    if (wCmd > L.turn_deg_s) wCmd = L.turn_deg_s;
    if (wCmd < TURN_MIN_DEG_S) wCmd = TURN_MIN_DEG_S;
    w += 4.0f * L.turn_deg_s * dt;                           // ramp up over a quarter second
    if (w > wCmd) w = wCmd;

    const float vWheel = w * DEG2RAD * TRACK_WIDTH_MM * 0.5f;
    wheelsDrive(-dir * vWheel, dir * vWheel, dt);

    if (runAbortRequested()) { motorsBrake(); return false; }
    if (millis() - t0 > MOVE_TIMEOUT_MS) { motorsBrake(); errorText = "turn timed out (gyro not counting or wheels stalled)"; return false; }
  }
  motorsBrake();
  delay(SETTLE_MS);

  const float encDeg = (float)((encRight() - r0) - (encLeft() - l0)) * mmPerTick() / TRACK_WIDTH_MM * RAD2DEG;
  if (fabsf(encDeg - dir * yawDeg) > 15.0f)
    Serial.printf("turn check: gyro %.1f deg, encoders %.1f deg - check GYRO_YAW_SIGN, ENC signs and TRACK_WIDTH_MM\n", dir * yawDeg, encDeg);
  tofService();
  return true;
}

bool turnLeft(int level)   { return pivot(+90.0f, level); }
bool turnRight(int level)  { return pivot(-90.0f, level); }
bool turnAround(int level) { return pivot(+180.0f, level); }

// ============================================================ BATTERY
float batteryVolts() {
  if (PIN_BATTERY_ADC < 0) return 0.0f;
  uint32_t sum = 0;
  for (int i = 0; i < 8; ++i) sum += analogReadMilliVolts(PIN_BATTERY_ADC);
  return (float)sum / 8.0f * 1e-3f * BATTERY_DIVIDER_RATIO;
}

// ============================================================ LIFECYCLE
const char* lastError() { return errorText; }

const char* begin() {
  boardUiBegin();
  led(0, 0, 0);
  const char* firstFailure = nullptr;

  motorsInit();                                    // outputs low = braked, before anything else
  Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL, I2C_HZ);
  i2cScan();
  const char* e = tofBoot(); if (e && !firstFailure) firstFailure = e;
  e = imuBoot();             if (e && !firstFailure) firstFailure = e;
  i2cScan();                                       // evidence for the log: expect 0x30 0x31 0x32 and 0x68/0x69
  encodersInit();
  if (PIN_BATTERY_ADC >= 0) { pinMode(PIN_BATTERY_ADC, INPUT); Serial.printf("battery: %.2f V\n", batteryVolts()); }
  if (imu) calibrateGyro();

  hardwareOk = (firstFailure == nullptr);
  if (!hardwareOk) errorText = firstFailure;
  return firstFailure;
}

const char* runBegin(int) {
  if (!hardwareOk) { errorText = "hardware init failed at power-up: fix it and reset"; return errorText; }
  if (ENCODER_TICKS_PER_REV <= 0.0f || WHEEL_DIAMETER_MM <= 0.0f || TRACK_WIDTH_MM <= 0.0f) {
    errorText = "not calibrated: set ENCODER_TICKS_PER_REV, WHEEL_DIAMETER_MM and TRACK_WIDTH_MM in config.h";
    return errorText;
  }
  const float vbat = batteryVolts();
  dutyCap = (vbat > MOTOR_RATED_V) ? (MOTOR_RATED_V / vbat) : MOTOR_DUTY_CAP;
  if (dutyCap > 1.0f) dutyCap = 1.0f;
  motorsBrake();
  wheelsReset();
  yawDeg = 0.0f;
  for (int i = 0; i < 3; ++i) tofAt[i] = 0;        // force fresh readings before the first move
  return nullptr;
}

void runEnd() {
  motorsBrake();
  delay(100);
}

// ============================================================ BENCH
void benchWalls() {
  Serial.println("walls: 20 samples (mm), then the decision after hysteresis. Hold a wall at maze distance.");
  for (int n = 0; n < 20; ++n) {
    const uint32_t t0 = millis();
    while (millis() - t0 < 100) { tofService(); delay(2); }
    bool l, f, r;
    const bool ok = readWalls(l, f, r);
    Serial.printf("L=%4u F=%4u R=%4u mm  ->  %s%s%s%s\n", tofMm[LEFT], tofMm[FRONT], tofMm[RIGHT],
                  ok ? "" : "(stale) ", l ? "L " : "- ", f ? "F " : "- ", r ? "R" : "-");
  }
}

void benchEncoders() {
  Serial.println("encoders: roll each wheel FORWARD by hand for 5 s; the count must go UP. One full turn = ticks per rev.");
  const int32_t l0 = encLeft(), r0 = encRight();
  for (int n = 0; n < 20; ++n) {
    delay(250);
    Serial.printf("left %ld  right %ld\n", (long)(encLeft() - l0), (long)(encRight() - r0));
  }
}

void benchImu() {
  if (!imu) { Serial.println("imu: not initialised"); return; }
  Serial.println("imu: 3 s. Turn the mouse LEFT by hand; yaw must go POSITIVE.");
  float yaw = 0.0f; uint32_t lastUs = micros();
  for (int n = 0; n < 30; ++n) {
    const uint32_t t0 = millis();
    while (millis() - t0 < 100) { const float dt = controlTick(lastUs); yaw += yawRateDps() * dt; }
    Serial.printf("rate %7.1f deg/s   yaw %7.1f deg\n", yawRateDps(), yaw);
  }
}

// H2: wheels off the table, one motor at a time, about a quarter of the cap.
void benchMotors() {
  Serial.println("motor test: WHEELS OFF THE TABLE. Each wheel: forward 1 s, stop, reverse 1 s. Forward must count UP on 'e'.");
  for (int w = 0; w < 2; ++w) {
    const char* name = w ? "right" : "left";
    const int32_t c0 = w ? encRight() : encLeft();
    Serial.printf("%s forward\n", name);  motorSet(w == 1, +0.25f); delay(1000); motorsBrake(); delay(500);
    const int32_t c1 = w ? encRight() : encLeft();
    Serial.printf("%s reverse\n", name);  motorSet(w == 1, -0.25f); delay(1000); motorsBrake(); delay(500);
    const int32_t c2 = w ? encRight() : encLeft();
    Serial.printf("%s: forward counted %ld, reverse counted %ld%s\n", name, (long)(c1 - c0), (long)(c2 - c1),
                  (c1 - c0) < 0 ? "  <- counts DOWN going forward: flip ENC sign or MOTOR invert" : "");
  }
}

void benchBattery() {
  if (PIN_BATTERY_ADC < 0) Serial.println("battery: no monitor fitted (PIN_BATTERY_ADC = -1)");
  else Serial.printf("battery: %.2f V  (duty cap for the next run %.0f%%)\n", batteryVolts(), 100.0f * ((batteryVolts() > MOTOR_RATED_V) ? MOTOR_RATED_V / batteryVolts() : MOTOR_DUTY_CAP));
}

void dryRunAttach(const mm::Mouse*) {}

} // namespace hal
