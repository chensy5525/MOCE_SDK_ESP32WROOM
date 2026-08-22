#ifndef ALARM_CAR_CONTROLLER_H
#define ALARM_CAR_CONTROLLER_H

#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Future IMU task calls this non-blocking API after shake detection. Not ISR-safe. */
esp_err_t alarm_car_request_stop(void);

/** Product audio/motion hooks. Implementations must be bounded and non-blocking. */
void alarm_car_on_alarm_triggered(uint8_t schedule_id);
void alarm_car_on_alarm_stopped(uint8_t schedule_id);

#ifdef __cplusplus
}
#endif

#endif
