/**
 * @file    <abbr>.h
 * @brief   <Module Name> public driver-board API skeleton
 */

#ifndef <FILENAME_UPPER>_H__
#define <FILENAME_UPPER>_H__

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Public headers remain platform-neutral.  Direct implementations use the
 * current BSP GPIO/PWM APIs.  A bridge version is generated only after its
 * CH32 firmware and ESP32 protocol layer actually support the required I/O. */

#define <ABBR>_OVERCURRENT_THRESHOLD_MA <val>U
#define <ABBR>_STALL_TIMEOUT_MS          <val>U
#define <ABBR>_DEAD_TIME_US              <val>U
#define <ABBR>_PWM_FREQ_HZ               <val>U

typedef enum {
    <ABBR>_MODE_PWM,
    <ABBR>_MODE_STEP,
    <ABBR>_MODE_SERVO,
} <abbr>_mode_t;

typedef struct {
    uint32_t pwm_freq_hz;
    uint32_t overcurrent_ma;
    uint32_t stall_timeout_ms;
    uint32_t dead_time_us;
    <abbr>_mode_t mode;
    /* Add verified transport-specific fields to the generated package. */
} <abbr>_cfg_t;

typedef struct <abbr>_ctx *<abbr>_handle_t;

int <abbr>_init(<abbr>_handle_t *handle, const <abbr>_cfg_t *cfg);
int <abbr>_deinit(<abbr>_handle_t handle);
int <abbr>_write(<abbr>_handle_t handle, int32_t speed, bool direction);
int <abbr>_read(<abbr>_handle_t handle, int32_t *rpm,
                uint32_t *fault_code);
int <abbr>_reset(<abbr>_handle_t handle);
int <abbr>_estop(<abbr>_handle_t handle);

#define ERR_<ABBR>_OVERCURRENT        -10
#define ERR_<ABBR>_STALL              -11
#define ERR_<ABBR>_OVERTEMP           -12
#define ERR_<ABBR>_FAULT_LOCKED       -13

#endif /* <FILENAME_UPPER>_H__ */
