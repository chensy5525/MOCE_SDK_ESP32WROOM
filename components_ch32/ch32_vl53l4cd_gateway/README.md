# CH32 VL53L4CD gateway driver

Independent VL53L4CD module driver over `ch32_i2c_multi_gateway_final`. It receives a stable F2-confirmed node reference and never owns TWAI, discovery, assignment or node IDs. Its 16-bit register access uses the generic write-prefix/read gateway API.

This is the only supported VL53 laser-ranging gateway package in the current
module library.
