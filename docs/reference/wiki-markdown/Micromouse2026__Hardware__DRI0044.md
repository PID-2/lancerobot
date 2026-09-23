# H2 - DFRobot DRI0044 Motor Driver

The DRI0044 drives **two brushed DC motors**. Each motor needs one direction signal and one PWM speed signal.

<div class="hardware-media-grid">
  <figure>
    <img src="https://dfimg.dfrobot.com/enshop/image/data/DRI0044/DRI0044_ShipList_06_360x240.jpg.webp" alt="DFRobot DRI0044 dual DC motor driver module">
    <figcaption>DFRobot DRI0044 module</figcaption>
  </figure>
  <figure>
    <img src="https://dfimg.dfrobot.com/nobody/wiki/482ccc756bc6e0a4a3b5057c3d58571a_0x0.png.webp" alt="DFRobot DRI0044 pinout showing logic, motor and power connections">
    <figcaption>Official DRI0044 pinout</figcaption>
  </figure>
</div>

| Item | DRI0044 rating |
|---|---|
| Motor channels | 2 |
| Continuous output | 1.2 A per channel |
| Logic supply `VCC` | 2.7-5.5 V |
| Motor supply `VM` | 2.5-12 V |

<p class="hardware-alert">⚠ Keep VCC and VM separate: VCC powers the control logic, while VM powers the motors. Never connect VM or a motor directly to an ESP32 GPIO.</p>

## Wiring

| DRI0044 | Connect to | Purpose |
|---|---|---|
| `VCC` | ESP32 `3V3` | logic power |
| `GND` | ESP32 ground and motor-supply ground | common reference |
| `DIR1` / `PWM1` | two chosen ESP32 GPIO outputs | left motor direction / speed |
| `DIR2` / `PWM2` | two chosen ESP32 GPIO outputs | right motor direction / speed |
| `VM` | motor battery or regulated motor supply | motor power |
| `M1+` / `M1-` | left motor | motor 1 output |
| `M2+` / `M2-` | right motor | motor 2 output |

Choose the four GPIO pins from your approved [ESP32-C6 pin map](#/docs/Micromouse2026/Hardware/ESP32C6.md). Do not copy random pin numbers from another robot.

![Official DFRobot DRI0044 Arduino wiring example](https://dfimg.dfrobot.com/nobody/wiki/b693da5c8ce58730c5ecd0a3e2b0b6ab_0x0.png.webp)

The image above shows the power paths and control signals. Your ESP32-C6 GPIO numbers will be different from the Arduino example.

## Test it before mounting

1. Raise the wheels off the table.
2. Check battery polarity, `VCC`, `VM` and the shared ground.
3. Start one motor at about 20-25% PWM.
4. Test forward, stop and reverse before enabling the second motor.
5. Stop if the ESP32 resets, the driver heats quickly or the current is unexpected.

If a wheel turns the wrong way, switch off power and swap that motor's two output wires, or invert its direction in software.

## Watch: TB6612FNG direction and PWM

<div class="video-frame">
  <iframe src="https://www.youtube.com/embed/eEf1n8TKNFQ" title="TB6612FNG motor driver direction and PWM tutorial" frameborder="0" allow="accelerometer; autoplay; clipboard-write; encrypted-media; gyroscope; picture-in-picture; web-share" referrerpolicy="strict-origin-when-cross-origin" allowfullscreen></iframe>
</div>

This video uses a common TB6612FNG breakout. For the DRI0044, follow the simpler `DIR1/PWM1` and `DIR2/PWM2` labels shown on this page.

## References

- [DFRobot DRI0044 product page](https://www.dfrobot.com/product-1704.html)
- [DFRobot DRI0044 wiki and pinout](https://wiki.dfrobot.com/dri0044/)
- [DFRobot DRI0044 schematic](https://dfimg.dfrobot.com/wiki/17474/DRI0044_tb6612-dual-dc-motor-driver-module_schematics_v1.0.pdf)
- [TB6612FNG datasheet](https://dfimg.dfrobot.com/wiki/17474/DRI0044_tb6612-dual-dc-motor-driver-module_datasheet_v1.0.pdf)