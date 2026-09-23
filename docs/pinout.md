# Pin map — ESP32-C6-DevKitC-1 Micromouse

Mirror of `esp32/micromouse_esp32/config.h`. Change both together. Status: **PROPOSED
2026-09-23, not yet verified on the bench.** Tick the last column as each wire is proven.

Sources for the GPIO facts: `docs/reference/datasheets/DevKitC1_user_guide.html.txt` (J1/J3
tables) and `docs/reference/diagrams/ESP32C6-DevKitC1-Pinout.png`. Component pins: Resource
Hub pages H2–H6 in `docs/reference/hub-pages/`.

## Board and revision

| | |
|---|---|
| Board | ESP32-C6-DevKitC-1 (module ESP32-C6-WROOM-1) |
| Revision | v1.2 per the user guide; **write the PW number printed on your board here:** ______ |
| Logic | 3.3 V only. Never 5 V on a GPIO. |
| Power in the mouse | buck converter 5.00 V → board **5V** pin. USB and the 5 V pin never together. |
| Arduino core | esp32 by Espressif Systems 3.x (compiled against 3.3.12); board "ESP32C6 Dev Module" |
| Libraries | VL53L0X by Pololu 1.3.1, MPU6050 by Electronic Cats 1.4.5 |

## Signals

| Signal | GPIO | Header | Device pin | Direction | Notes | Verified |
|---|---|---|---|---|---|---|
| Start button | 9 | on board | BOOT button | in, pull-up | strapping pin, only matters while held during reset | ☐ |
| Status LED | 8 | on board | RGB LED | out | addressable LED, driven by the core's `rgbLedWrite` | ☐ |
| I2C SDA | 23 | J3-5 | SDA on 3× GY-530 and SEN0142 | bidirectional | the core's default Wire pins for this board; pull-ups already on SEN0142 (4.7 kΩ), add none | ☐ |
| I2C SCL | 22 | J3-6 | SCL on 3× GY-530 and SEN0142 | out | | ☐ |
| Left XSHUT | 0 | J1-7 | left GY-530 XSHUT | out | LOW = sensor off; address 0x30 after boot | ☐ |
| Front XSHUT | 1 | J1-8 | front GY-530 XSHUT | out | address 0x31 | ☐ |
| Right XSHUT | 2 | J1-12 | right GY-530 XSHUT | out | address 0x32 | ☐ |
| Left motor PWM | 10 | J1-10 | DRI0044 PWM1 | out, 20 kHz | PWM 0 = short brake on this module | ☐ |
| Left motor DIR | 11 | J1-11 | DRI0044 DIR1 | out | flip `MOTOR_LEFT_INVERT` if the wheel runs backwards | ☐ |
| Right motor PWM | 6 | J1-5 | DRI0044 PWM2 | out, 20 kHz | | ☐ |
| Right motor DIR | 7 | J1-6 | DRI0044 DIR2 | out | | ☐ |
| Left encoder A | 18 | J3-10 | left motor encoder A | in, interrupt | encoder VCC from 3V3 only if the encoder supports it | ☐ |
| Left encoder B | 19 | J3-9 | left motor encoder B | in, interrupt | | ☐ |
| Right encoder A | 20 | J3-8 | right motor encoder A | in, interrupt | | ☐ |
| Right encoder B | 21 | J3-7 | right motor encoder B | in, interrupt | | ☐ |
| Battery monitor | 3 (spare) | J1-13 | resistor divider from the pack | in, ADC1_CH3 | optional; `PIN_BATTERY_ADC` is -1 until fitted | ☐ |

Power and ground (not GPIO): DRI0044 `VCC` → 3V3, `VM` → motor supply, all `GND` common;
SEN0142 `VIN` → 3V3; GY-530 `VCC` → 3V3; encoder `VCC` → 3V3 (if 3.3 V capable).

## Pins deliberately not used

| GPIO | Why |
|---|---|
| 4, 5 | strapping (MTMS, MTDI) |
| 15 | strapping |
| 12, 13 | native USB D−/D+ |
| 16, 17 | UART0 to the USB-serial bridge (the serial monitor) |

Free pins after this map: none besides GPIO3, reserved above. If the team adds a device, the
candidates are the IMU `INT` line (not needed by the firmware) or moving the serial monitor to the
native USB port to free 16 and 17.

## Bench procedure (H1 §7, in order)

1. No power: continuity check for shorts, motors unplugged from the driver.
2. USB only: flash the dry run (`HARDWARE_READY 0`), open the monitor at 115200, press the button.
3. `HARDWARE_READY 1`: one I2C device at a time, watch the boot log for `init: OK` lines.
4. Motors, wheels off the table: `m` on the console.
5. Encoders: `e`, then fill in `ENCODER_TICKS_PER_REV` and the signs.
6. IMU: `i`, then `GYRO_YAW_SIGN`.
7. Walls on the real maze: `w`, then the thresholds.
