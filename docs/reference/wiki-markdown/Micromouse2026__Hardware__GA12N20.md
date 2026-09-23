# H6 - 2x GA12-N20 Micro Gear Motors with Hall Encoders

Each wheel uses a **6 V GA12-N20 gearmotor** rated at about **500 rpm with no load**. The two motor wires create motion; the Hall encoder's `A` and `B` signals measure wheel movement and direction.

<figure>
  <img src="../../assets/images/GA12-N20-dual-encoder-wiring.svg" alt="Two GA12-N20 motors connected to a DRI0044 motor driver, with both Hall encoders connected to an ESP32-C6">
  <figcaption>Keep motor power on the driver side and encoder signals on the 3.3 V logic side.</figcaption>
</figure>

| Item | This build |
|---|---|
| Quantity | 2 motors, one per wheel |
| Motor rating | 6 V, about 500 rpm no-load |
| Feedback | two-channel Hall encoder (`A` and `B`) |
| ESP32 inputs | 4 GPIO inputs total |

<p class="hardware-alert">⚠ A fully charged 2S battery is 8.4 V, which is above the motor's 6 V rating. Use a suitable motor supply or confirm an approved voltage limit before connecting `VM`.</p>

## Identify the six connections

Encoder versions normally expose these six functions, but **wire colours and connector order vary**. Check the label or seller pinout for your exact motor.

| Motor connection | Connect to |
|---|---|
| `M+`, `M-` | one DRI0044 motor-output pair |
| Encoder `VCC` | `3V3`, only if your encoder supports 3.3 V |
| Encoder `GND` | common ground |
| Encoder `A`, `B` | two chosen ESP32-C6 GPIO inputs |

Repeat the same connections for the second motor. Both encoder grounds must share ground with the ESP32-C6 and motor driver.

<p class="hardware-alert">⚠ ESP32 GPIO must stay at or below 3.6 V. Power a compatible encoder from 3.3 V, or level-shift its outputs. Never allow a 5 V encoder signal to reach a GPIO directly.</p>

## What A and B tell you

The encoder produces two square waves that are offset from each other:

- pulse count gives relative wheel movement;
- pulse frequency gives wheel speed;
- whether `A` leads `B` or `B` leads `A` gives direction.

Do not assume a fixed number of ticks per wheel revolution. Gear ratio, encoder version and whether software counts one, two or four edges all change the result.

## First test

1. Lift both wheels off the table and keep motor power off.
2. Power the encoder from the verified logic voltage.
3. Turn one wheel by hand and confirm both `A` and `B` counters change.
4. Run that motor slowly through the DRI0044 and check the count direction.
5. Repeat for the second wheel, then record the measured ticks per wheel revolution.

For distance calibration:

```text
distance per tick = (pi x wheel diameter) / measured ticks per wheel revolution
```

If one wheel reports the opposite sign, correct that wheel's direction in software. Do not swap encoder power wires.

## References

- [Example GA12-N20 6 V 500 rpm Hall-encoder specification](https://abra-electronics.com/electromechanical/motors/gear-motors/mini-metal-gearmotors/ga12-n20-6v500-zh-jga12-n20-6v-hall-encoded-metal-gear-dc-motor-500-rpm-with-jst-zh-6-pin-connector.html)
- [Espressif GPIO voltage guidance](https://docs.espressif.com/projects/esp-faq/en/latest/hardware-related/hardware-design.html#what-is-the-voltage-tolerance-of-gpios-of-esp-chips)
- [H2 - DFRobot DRI0044 Motor Driver](#/docs/Micromouse2026/Hardware/DRI0044.md)
- [H1 - ESP32-C6 Controller Guide](#/docs/Micromouse2026/Hardware/ESP32C6.md)