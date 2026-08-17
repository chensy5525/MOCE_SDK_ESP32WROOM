# VL53L4CD CH32 bridge test

The ESP32 dynamically discovers and F2-confirms CH32-I2C nodes in one
persistent node table. It tries candidates in order and retains the first
VL53L4CD whose live probe and initialization succeed at downstream address
0x29 (`0x010F = 0xEBAA`). A missing or recovering role first uses the bounded
250 ms/500 ms/1 s/2 s retry burst, then falls back to the 10-second steady
missing-role interval. The selected sensor is sampled on a fixed 5 Hz
start-to-start schedule; missed periods are skipped rather than replayed.
`OUT RANGE` is a completed measurement and does not count as a communication
failure. Three consecutive communication/data-ready failures release the local
handle, run incremental discovery, reinitialize only this sensor and resume
sampling. Initialization has one 3.5-second total deadline and each public
read has one 400 ms total deadline. The observation window lasts 60 seconds
from the first successful bind, after which the sensor is deinitialized and the
example returns.

This example requires the updated generic `WRITE_READ` capability in
`CH32_I2C_gateway_dynamic`. VL53L4CD is the supported laser-ranging chip in the
current module library.
