#include "vl53l0x_direct_policy.h"

vl53l0x_direct_range_class_t vl53l0x_direct_classify_raw(uint16_t raw_distance_mm)
{
    if (raw_distance_mm == 0U || raw_distance_mm == 0xFFFFU ||
        raw_distance_mm > 2000U) {
        return VL53L0X_DIRECT_RANGE_OUT_OF_RANGE;
    }
    return VL53L0X_DIRECT_RANGE_VALID;
}
