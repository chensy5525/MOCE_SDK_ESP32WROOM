# VL53L4CD direct test

Connect 3V3, GND, SDA GPIO21 and SCL GPIO22. The example verifies `0x010F = 0xEBAA` and prints five measurements per second for about ten seconds. Results beyond 1300 mm or with a non-zero range status print `OUT RANGE`.

The separate VL53L0X direct package remains unchanged.
