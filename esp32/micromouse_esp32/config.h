// config.h — Micromouse ESP32-C6: pin map, electrical limits and calibration constants.
//
// THE SINGLE SOURCE OF TRUTH for every hardware number in the firmware.
// docs/pinout.md mirrors this file; change both together.
//
// Every GPIO fact below comes from the ESP32-C6-DevKitC-1 user guide (J1/J3 pin
// tables) and the pinout diagram on Resource Hub page H1, both kept in
// docs/reference/. Component facts cite their Resource Hub page (H2..H6) or
// the schematic/datasheet in docs/reference/datasheets/.
//
// Pin ASSIGNMENTS are a proposal (2026-09-23) chosen from the free pins; the
// facts about which pins are free are not. Verify each wire on the bench
// before setting HARDWARE_READY to 1.

#pragma once
#include <stdint.h>

// 0 = dry run: no motors, no sensors; the solver walks a built-in maze and
//     prints every move. Safe to flash on day one.
// 1 = real hardware layer (hal_esp32c6.h). Complete the CALIBRATE items below first.
#ifndef HARDWARE_READY
#define HARDWARE_READY 0
#endif
#if HARDWARE_READY
#define HARDWARE_READY_NAME "hardware layer: real mouse"
#else
#define HARDWARE_READY_NAME "DRY RUN: no motors or sensors, walking the built-in UK 2016 maze"
#endif

// ============================================================ TEAM PIN MAP
// Excluded on purpose (user guide): strapping pins GPIO4, 5, 8, 9, 15; native
// USB GPIO12/13; UART0 GPIO16/17 (the USB serial monitor). GPIO8 and GPIO9 are
// used only through the parts the board already wires to them.
//
// Free header pins after the exclusions: 0 1 2 3 6 7 10 11 18 19 20 21 22 23 (14).
// This map uses 13 of them and leaves GPIO3 (ADC1_CH3) for the battery monitor.

// On-board user interface (nothing to wire).
constexpr int PIN_BUTTON  = 9;   // BOOT button, to GND, read with INPUT_PULLUP. Strapping pin: only matters while held during reset.
constexpr int PIN_STATUS_LED = 8;   // addressable RGB LED "driven by GPIO8" (user guide)

// I2C bus shared by the three VL53L0X boards and the SEN0142 (H3, H4). These are
// the Arduino core's default Wire pins for this board, so library examples that
// call Wire.begin() with no arguments work on the bench without edits.
constexpr int PIN_I2C_SDA = 23;  // J3 pin 5
constexpr int PIN_I2C_SCL = 22;  // J3 pin 6

// VL53L0X XSHUT, one dedicated output each (H4). Active LOW = sensor held in shutdown.
constexpr int PIN_XSHUT_LEFT  = 0;   // J1 pin 7
constexpr int PIN_XSHUT_FRONT = 1;   // J1 pin 8
constexpr int PIN_XSHUT_RIGHT = 2;   // J1 pin 12

// DRI0044 motor driver (H2): one direction + one PWM line per motor. VCC -> 3V3.
constexpr int PIN_MOTOR_L_PWM = 10;  // J1 pin 10  -> PWM1
constexpr int PIN_MOTOR_L_DIR = 11;  // J1 pin 11  -> DIR1
constexpr int PIN_MOTOR_R_PWM = 6;   // J1 pin 5   -> PWM2
constexpr int PIN_MOTOR_R_DIR = 7;   // J1 pin 6   -> DIR2

// GA12-N20 Hall encoders, A and B per wheel (H6). Encoder VCC from 3V3 only if the
// encoder supports 3.3 V; a signal above 3.6 V must never reach a GPIO.
constexpr int PIN_ENC_L_A = 18;  // J3 pin 10
constexpr int PIN_ENC_L_B = 19;  // J3 pin 9
constexpr int PIN_ENC_R_A = 20;  // J3 pin 8
constexpr int PIN_ENC_R_B = 21;  // J3 pin 7

// Optional battery monitor through a resistor divider (H1). -1 = not fitted.
constexpr int   PIN_BATTERY_ADC     = -1;    // set to 3 (ADC1_CH3) once the divider exists
constexpr float BATTERY_DIVIDER_RATIO = 3.0f; // Vbat / Vadc, from the resistor values fitted

// ============================================================ ELECTRICAL LIMITS
constexpr float    MOTOR_RATED_V  = 6.0f;                          // H6: GA12-N20 is a 6 V motor
constexpr float    BATTERY_FULL_V = 8.4f;                          // H5: 2S pack, 8.4 V full
constexpr float    MOTOR_DUTY_CAP = MOTOR_RATED_V / BATTERY_FULL_V; // 0.71: the "approved limit" H6 asks for, applied in software
constexpr uint32_t MOTOR_PWM_HZ   = 20000;                         // TB6612FNG datasheet: fPWM <= 100 kHz; 20 kHz is above hearing
constexpr uint8_t  MOTOR_PWM_BITS = 10;                            // 0..1023

constexpr uint32_t I2C_HZ = 400000;
constexpr uint8_t  I2C_ADDR_TOF_BOOT  = 0x29;  // every VL53L0X powers up here (H4)
constexpr uint8_t  I2C_ADDR_TOF_LEFT  = 0x30;  // assigned at every boot, in this order (H4)
constexpr uint8_t  I2C_ADDR_TOF_FRONT = 0x31;
constexpr uint8_t  I2C_ADDR_TOF_RIGHT = 0x32;
constexpr uint8_t  I2C_ADDR_IMU_AD0_LOW  = 0x68; // H3. SEN0142 schematic: AD0 sits on a solder jumper,
constexpr uint8_t  I2C_ADDR_IMU_AD0_HIGH = 0x69; // so 0x69 is possible; the firmware scans both.

// ============================================================ MAZE GEOMETRY (S1, UKMARS)
constexpr float CELL_MM      = 180.0f;  // centre to centre
constexpr float PASSAGE_MM   = 168.0f;  // between wall faces

// ============================================================ CALIBRATE: motion
// Measured values only. Zero means "not measured yet" and hal::begin() refuses
// to start a run (bench commands still work).
constexpr float ENCODER_TICKS_PER_REV = 0.0f;   // CALIBRATE (H6): turn the wheel one revolution by hand and read 'e'
constexpr float WHEEL_DIAMETER_MM     = 0.0f;   // CALIBRATE: measure the kit wheel
constexpr float TRACK_WIDTH_MM        = 0.0f;   // CALIBRATE: distance between the two tyre contact patches
constexpr int   ENC_LEFT_SIGN         = +1;     // CALIBRATE (H6): +1 or -1 so that rolling forward counts UP
constexpr int   ENC_RIGHT_SIGN        = +1;
constexpr bool  MOTOR_LEFT_INVERT     = false;  // CALIBRATE (H2): flip if the wheel runs backwards at DIR=HIGH
constexpr bool  MOTOR_RIGHT_INVERT    = false;
constexpr int   GYRO_YAW_SIGN         = +1;     // CALIBRATE (H3): +1 or -1 so that a LEFT turn reads positive

// Speed ladder (S2): level 1 = search crawl, 5 = flat out. Starting points; tune on the practice maze.
struct SpeedLevel { float cruise_mm_s; float accel_mm_s2; float turn_deg_s; };
constexpr SpeedLevel SPEED_LEVELS[5] = {
  { 200.0f,  600.0f, 120.0f },
  { 300.0f,  900.0f, 180.0f },
  { 400.0f, 1200.0f, 240.0f },
  { 500.0f, 1500.0f, 300.0f },
  { 600.0f, 1800.0f, 360.0f },
};

// Control loop and gains (CALIBRATE on the bench, wheels off the table first).
constexpr uint32_t CONTROL_PERIOD_US   = 5000;   // 200 Hz
constexpr float    WHEEL_KP            = 0.0025f; // motor command per mm/s of speed error
constexpr float    WHEEL_KI            = 0.02f;   // per mm of accumulated error, per second
constexpr float    WHEEL_KFF           = 0.0016f; // feed-forward: command per mm/s (about 1/max no-load speed)
constexpr float    WALL_KP             = 4.0f;    // steering: mm/s of wheel-speed difference per mm of left-right error
constexpr float    HEADING_KP          = 8.0f;    // steering: mm/s per degree of heading error when no side wall is visible
constexpr float    TURN_KP             = 6.0f;    // deg/s per degree remaining during a pivot
constexpr float    TURN_MIN_DEG_S      = 40.0f;   // floor so the pivot always finishes
constexpr float    TURN_DONE_DEG       = 1.5f;    // pivot finishes inside this window
constexpr uint32_t MOVE_TIMEOUT_MS     = 4000;    // any single move longer than this is a fault
constexpr uint32_t SETTLE_MS           = 60;      // rest after each move before reading walls

// ============================================================ CALIBRATE: sensors
// Wall thresholds in mm with hysteresis (S4, H4). Set them on the real maze under the real lights.
constexpr uint16_t WALL_SIDE_ON_MM   = 100;  // side wall PRESENT below this...
constexpr uint16_t WALL_SIDE_OFF_MM  = 130;  // ...ABSENT above this; in between keep the last decision
constexpr uint16_t WALL_FRONT_ON_MM  = 120;
constexpr uint16_t WALL_FRONT_OFF_MM = 160;
constexpr uint16_t TOF_VALID_MAX_MM  = 1000; // above this a reading means "nothing in range", not a wall
constexpr uint16_t SIDE_CENTRED_MM   = 65;   // CALIBRATE: side reading when the mouse is centred in a corridor
constexpr uint16_t FRONT_STOP_MM     = 60;   // CALIBRATE: front reading when the mouse is at a cell centre facing a wall
constexpr uint32_t TOF_TIMING_BUDGET_US = 20000; // Pololu: default 33 ms, minimum 20 ms
constexpr uint32_t TOF_STALE_MS      = 150;  // a reading older than this is not trusted
constexpr uint16_t GYRO_CAL_SAMPLES  = 400;  // bias average at boot, mouse absolutely still (H3, S4)

// ============================================================ RUN CONTROL
constexpr uint32_t BUTTON_DEBOUNCE_MS     = 30;
constexpr uint32_t BUTTON_LONG_MS         = 1500;  // hold this long: next speed level
constexpr uint32_t BUTTON_FORGET_MS       = 5000;  // hold this long: forget the maze (rules: a judge-requested recovery erases the map)
constexpr uint32_t START_DELAY_MS         = 1500;  // hands clear before the wheels move
constexpr uint32_t REST_AFTER_RUN_MS      = 2000;  // UKMARS: stop >= 2 s in the start cell before the next run
constexpr uint32_t IDLE_BLINK_PERIOD_MS   = 3000;  // idle LED blinks the speed level this often
constexpr uint8_t  LED_BRIGHTNESS         = 40;    // 0..255, the on-board LED is bright
