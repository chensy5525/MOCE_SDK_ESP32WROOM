# VL53L4CD CH32 bridge test

The ESP32 dynamically discovers and F2-confirms CH32-I2C nodes, preserves stable node records and incrementally rediscovers them. A VL53L4CD at downstream address 0x29 is identified by `0x010F = 0xEBAA`, then sampled at 5 Hz for about ten seconds. Results beyond 1300 mm or with a non-zero range status print `OUT RANGE`.

This example requires the updated generic `WRITE_READ` capability in `CH32_I2C_gateway_dynamic`. The existing VL53L0X bridge package remains unchanged.
