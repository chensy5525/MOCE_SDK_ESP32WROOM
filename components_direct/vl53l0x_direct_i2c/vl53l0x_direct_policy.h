#pragma once

#include <stdint.h>

typedef enum {
    VL53L0X_DIRECT_RANGE_VALID = 0,
    VL53L0X_DIRECT_RANGE_OUT_OF_RANGE,
} vl53l0x_direct_range_class_t;

vl53l0x_direct_range_class_t vl53l0x_direct_classify_raw(uint16_t raw_distance_mm);
