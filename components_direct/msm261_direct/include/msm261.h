#ifndef MSM261_H__
#define MSM261_H__

#include <stdbool.h>
#include <stdint.h>

#ifndef ERR_TIMEOUT
#define ERR_TIMEOUT                 -1
#define ERR_BUSY                    -2
#define ERR_NOT_INIT                -3
#define ERR_INVALID_PARAM           -4
#define ERR_NOT_SUPPORTED           -5
#define ERR_OVERFLOW                -6
#define ERR_NO_DEVICE               -7
#define ERR_HW_FAULT                -8
#endif

#define MSM261_SAMPLE_RATE_HZ               16000U
#define MSM261_WINDOW_MS                    100U
#define MSM261_WINDOW_SAMPLES               1600U
#define MSM261_STARTUP_MS                   20U
#define MSM261_MODE_CHANGE_MS               10U
#define MSM261_READ_TIMEOUT_MS              200U
#define MSM261_DMA_DESC_NUM                 6U
#define MSM261_DMA_FRAME_NUM                320U
#define MSM261_STUCK_BLOCK_LIMIT            3U
#define MSM261_DBFS_FLOOR                   (-120.0F)

#define ERR_MSM261_I2S_CONFIG               -10
#define ERR_MSM261_READ_TIMEOUT             -11
#define ERR_MSM261_SHORT_READ               -12
#define ERR_MSM261_STREAM_OVERRUN            -13
#define ERR_MSM261_STUCK_DATA                -14

typedef enum {
    MSM261_SLOT_SELECT_LOW = 0,
    MSM261_SLOT_SELECT_HIGH = 1,
    MSM261_SLOT_AUTO = 2,
} msm261_slot_t;

typedef struct {
    int clk_gpio;
    int data_gpio;
    msm261_slot_t slot;
    bool invert_clock;
    uint32_t timeout_ms;
} msm261_cfg_t;

typedef struct {
    int16_t peak;
    float rms;
    float dbfs;
    int16_t dc_mean;
    uint32_t sample_count;
    uint32_t clipped_samples;
    uint32_t ok_count;
    uint32_t error_count;
    bool valid;
} msm261_level_t;

typedef struct msm261_ctx *msm261_handle_t;

void msm261_default_config(msm261_cfg_t *config);
int msm261_init(msm261_handle_t *handle, const msm261_cfg_t *config);
int msm261_read_level(msm261_handle_t handle, msm261_level_t *level);
int msm261_deinit(msm261_handle_t handle);

#endif /* MSM261_H__ */
