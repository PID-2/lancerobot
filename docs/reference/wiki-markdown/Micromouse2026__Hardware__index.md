# Build your hardware

Get the controller, motors and sensors working one at a time. Start with H1 to identify your board and make a pin map; use H5 before powering the controller from a battery.

<nav class="hardware-directory" aria-label="Hardware guides">
<a href="#/docs/Micromouse2026/Hardware/ESP32C6.md"><span class="guide-code">H1</span><strong>ESP32-C6 controller →</strong><span class="guide-description">Choose your pins, plan the wiring and bring the controller online.</span></a>
<a href="#/docs/Micromouse2026/Hardware/DRI0044.md"><span class="guide-code">H2</span><strong>Motor driver →</strong><span class="guide-description">Connect the DRI0044 and test each wheel’s direction and speed.</span></a>
<a href="#/docs/Micromouse2026/Hardware/SEN0142.md"><span class="guide-code">H3</span><strong>Motion sensor →</strong><span class="guide-description">Wire the SEN0142 IMU, check its orientation and calibrate gyro bias.</span></a>
<a href="#/docs/Micromouse2026/Hardware/VL53L0X.md"><span class="guide-code">H4</span><strong>Distance sensors →</strong><span class="guide-description">Connect three VL53L0X boards and give each a unique I²C address.</span></a>
<a href="#/docs/Micromouse2026/Hardware/BuckConverter.md"><span class="guide-code">H5</span><strong>Battery & power →</strong><span class="guide-description">Set and verify a 5 V supply before connecting the controller.</span></a>
<a href="#/docs/Micromouse2026/Hardware/GA12N20.md"><span class="guide-code">H6</span><strong>2x GA12-N20 micro gear motors →</strong><span class="guide-description">Connect both motor channels and verify the two Hall encoders.</span></a>
</nav>

## Before you connect anything

Switch off power before changing wires. Keep your board’s pin map beside you, and check each connection against the guide for that component.