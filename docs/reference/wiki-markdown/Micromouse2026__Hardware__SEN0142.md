# H3 - DFRobot SEN0142 IMU

The SEN0142 combines a **3-axis accelerometer** and **3-axis gyroscope**. A Micromouse can use it to measure turns, vibration and changes in motion.

<div class="hardware-media-grid">
  <figure>
    <img src="https://dfimg.dfrobot.com/enshop/SEN0142/SEN0142_Main_01_360x240.jpg.webp" alt="DFRobot SEN0142 MPU-6050 six-axis IMU module">
    <figcaption>DFRobot SEN0142 module</figcaption>
  </figure>
  <figure>
    <img src="https://dfimg.dfrobot.com/99f64da1591d41a88712e46e2feb7a6b/wikien/e14c576930ba8daea73f5c38214275a5_0x0.png.webp" alt="Official DFRobot SEN0142 Arduino I2C wiring example">
    <figcaption>Official I2C wiring example</figcaption>
  </figure>
</div>

| Item | SEN0142 |
|---|---|
| Sensor | MPU-6050, 6DOF |
| Interface | I2C |
| Working voltage | 3-5 V |
| Acceleration ranges | ±2g, ±4g, ±8g, ±16g |
| Gyroscope ranges | ±250, ±500, ±1000, ±2000°/s |

<p class="hardware-alert">⚠ Power the SEN0142 from the ESP32-C6 3V3 rail, record the board's X/Y/Z orientation and calibrate it while completely still. A rotated board or uncorrected gyro bias will give the robot the wrong turn direction.</p>

## Wiring

| SEN0142 | ESP32-C6 | Purpose |
|---|---|---|
| `VIN` | `3V3` | sensor power |
| `GND` | `GND` | common reference |
| `SDA` | chosen I2C SDA GPIO | data |
| `SCL` | chosen I2C SCL GPIO | clock |
| `INT` | optional GPIO input | data-ready / motion interrupt |
| `AUX_DA`, `AUX_CL` | leave unconnected | auxiliary sensor bus |

Choose SDA, SCL and optional INT from your approved [ESP32-C6 pin map](#/docs/Micromouse2026/Hardware/ESP32C6.md). The official Arduino image shows the signal names, not the GPIO numbers for your ESP32-C6.

## Check and calibrate

1. Fix the board flat and note which arrow or board edge points forward.
2. Run an I2C scanner; check address `0x68` first.
3. Keep the mouse still and record gyro offsets for a few seconds.
4. Tilt and rotate one axis at a time; confirm the sign matches your robot coordinate system.

```text
Robot forward: +X =
Robot left:    +Y =
Robot up:      +Z =
Gyro bias: gx =       gy =       gz =
```

{: .warning}
> The MPU-6050 has no magnetometer. Gyroscope-based yaw will drift over time, so combine it with wheel encoders and wall sensors instead of treating it as an absolute compass.

## Watch: why IMU drift matters

<div class="video-frame">
  <iframe src="https://www.youtube.com/embed/SeCtWOr-pwU" title="Drift of Arduino DFRobot 6DOF module" frameborder="0" allow="accelerometer; autoplay; clipboard-write; encrypted-media; gyroscope; picture-in-picture; web-share" referrerpolicy="strict-origin-when-cross-origin" allowfullscreen></iframe>
</div>

This video, embedded on DFRobot's SEN0142 product page, shows why gyro calibration and sensor fusion matter. Use the official wiring image above for the actual connections.

## References

- [DFRobot SEN0142 product page](https://www.dfrobot.com/product-880.html)
- [DFRobot SEN0142 wiki](https://wiki.dfrobot.com/sen0142)
- [DFRobot reading and wiring example](https://wiki.dfrobot.com/sen0142/docs/18851)
- [SEN0142 schematic](https://dfimg.dfrobot.com/wiki/18671/SEN0142_mpu-6050-6-dof-sensor_schematics_V1.0.pdf)
- [MPU-6050 product specification](https://image.dfrobot.com/image/data/SEN0142/PS-MPU-6000A.pdf)