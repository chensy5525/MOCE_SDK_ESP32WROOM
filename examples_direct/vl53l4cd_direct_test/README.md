# VL53L4CD direct test

Connect 3V3, GND, SDA GPIO21 and SCL GPIO22. The example verifies
`0x010F = 0xEBAA`, samples for 60 seconds at a 200 ms period, and prints every
fifth result. It then deinitializes and returns. Results beyond
1300 mm or with a non-zero range status print `OUT RANGE`. Each public read has
one 250 ms total deadline shared by polling, retries and result-register access.
The complete initialization sequence has one 3.5-second deadline shared by
firmware-ready polling, default-configuration chunks, calibration ranging and
all configuration-register operations.

VL53L4CD is the supported laser-ranging chip in the current module library.
