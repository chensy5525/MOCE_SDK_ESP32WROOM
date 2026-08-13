#include "msm261.h"

#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "board.h"
#include "bsp_i2s.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define MSM261_TAG                         "MSM261"
#define MSM261_CLIP_THRESHOLD              32760
#define MSM261_DISCARD_BLOCK_SAMPLES       320U
#define MSM261_SLOT_PROBE_ATTEMPTS          2U
#define MSM261_SLOT_VALIDATE_BLOCKS         2U

#define MSM261_LOG_INF(format, ...) \
    printf("[INF][" MSM261_TAG "] " format "\n", ##__VA_ARGS__)
#define MSM261_LOG_WRN(format, ...) \
    printf("[WRN][" MSM261_TAG "] " format "\n", ##__VA_ARGS__)
#define MSM261_LOG_ERR(format, ...) \
    printf("[ERR][" MSM261_TAG "] " format "\n", ##__VA_ARGS__)

typedef struct msm261_ctx {
    bool initialized;
    msm261_cfg_t config;
    int16_t samples[MSM261_WINDOW_SAMPLES];
    uint32_t ok_count;
    uint32_t error_count;
    uint32_t stuck_block_count;
} msm261_ctx_t;

static msm261_ctx_t s_context;
static bool s_context_in_use;

static int msm261_map_esp_error(esp_err_t error)
{
    if (error == ESP_ERR_TIMEOUT) {
        return ERR_MSM261_READ_TIMEOUT;
    }
    if (error == ESP_ERR_INVALID_ARG) {
        return ERR_INVALID_PARAM;
    }
    if (error == ESP_ERR_INVALID_STATE) {
        return ERR_BUSY;
    }
    if (error == ESP_ERR_NOT_SUPPORTED) {
        return ERR_NOT_SUPPORTED;
    }
    return ERR_HW_FAULT;
}

void msm261_default_config(msm261_cfg_t *config)
{
    if (config == NULL) {
        return;
    }
    *config = (msm261_cfg_t) {
        .clk_gpio = BOARD_PDM_CLK_GPIO,
        .data_gpio = BOARD_PDM_DATA_GPIO,
        .slot = MSM261_SLOT_AUTO,
        .invert_clock = false,
        .timeout_ms = MSM261_READ_TIMEOUT_MS,
    };
}

int msm261_init(msm261_handle_t *handle, const msm261_cfg_t *config)
{
    bsp_i2s_pdm_rx_config_t bsp_config;
    int16_t discard_buffer[MSM261_DISCARD_BLOCK_SAMPLES];
    size_t samples_read = 0U;
    msm261_slot_t selected_slot = MSM261_SLOT_SELECT_LOW;
    uint32_t slot_attempt;
    uint32_t slot_attempt_count;
    esp_err_t error = ESP_OK;

    if (handle == NULL || config == NULL || config->clk_gpio < 0 ||
        config->data_gpio < 0 || config->clk_gpio == config->data_gpio ||
        config->timeout_ms == 0U ||
        (config->slot != MSM261_SLOT_SELECT_LOW &&
         config->slot != MSM261_SLOT_SELECT_HIGH &&
         config->slot != MSM261_SLOT_AUTO)) {
        MSM261_LOG_ERR("init FAIL, reason=invalid_param");
        return ERR_INVALID_PARAM;
    }
    *handle = NULL;
    if (s_context_in_use) {
        MSM261_LOG_ERR("init FAIL, reason=busy");
        return ERR_BUSY;
    }

    memset(&s_context, 0, sizeof(s_context));
    s_context.config = *config;
    s_context_in_use = true;
    bsp_config = (bsp_i2s_pdm_rx_config_t) {
        .port = BOARD_PDM_I2S_PORT,
        .clk_gpio = config->clk_gpio,
        .data_gpio = config->data_gpio,
        .sample_rate_hz = MSM261_SAMPLE_RATE_HZ,
        .slot = BSP_I2S_PDM_SLOT_SELECT_LOW,
        .invert_clock = config->invert_clock,
        .dma_desc_num = MSM261_DMA_DESC_NUM,
        .dma_frame_num = MSM261_DMA_FRAME_NUM,
    };
    slot_attempt_count = config->slot == MSM261_SLOT_AUTO
                             ? MSM261_SLOT_PROBE_ATTEMPTS : 1U;
    for (slot_attempt = 0U; slot_attempt < slot_attempt_count; ++slot_attempt) {
        int16_t minimum = 0;
        int16_t maximum = 0;
        uint32_t valid_blocks = 0U;
        uint32_t probe_block;

        selected_slot = config->slot == MSM261_SLOT_AUTO
                            ? (slot_attempt == 0U ? MSM261_SLOT_SELECT_LOW
                                                 : MSM261_SLOT_SELECT_HIGH)
                            : config->slot;
        bsp_config.slot = selected_slot == MSM261_SLOT_SELECT_HIGH
                              ? BSP_I2S_PDM_SLOT_SELECT_HIGH
                              : BSP_I2S_PDM_SLOT_SELECT_LOW;
        samples_read = 0U;
        error = bsp_i2s_pdm_rx_init(&bsp_config);
        if (error != ESP_OK) {
            s_context_in_use = false;
            MSM261_LOG_ERR("init FAIL, reason=i2s_config slot=%s esp_err=0x%X",
                           selected_slot == MSM261_SLOT_SELECT_HIGH ? "HIGH" : "LOW",
                           (unsigned)error);
            return error == ESP_ERR_NOT_SUPPORTED ? ERR_NOT_SUPPORTED
                                                  : ERR_MSM261_I2S_CONFIG;
        }

        vTaskDelay(pdMS_TO_TICKS(MSM261_STARTUP_MS + MSM261_MODE_CHANGE_MS));
        /* 丢弃启动后的首块，避免把滤波器瞬态误判为有效音频。 */
        error = bsp_i2s_pdm_rx_read(discard_buffer,
                                    MSM261_DISCARD_BLOCK_SAMPLES,
                                    &samples_read, config->timeout_ms);
        for (probe_block = 0U;
             error == ESP_OK &&
             samples_read == MSM261_DISCARD_BLOCK_SAMPLES &&
             probe_block < MSM261_SLOT_VALIDATE_BLOCKS;
             ++probe_block) {
            size_t index;

            minimum = INT16_MAX;
            maximum = INT16_MIN;
            samples_read = 0U;
            error = bsp_i2s_pdm_rx_read(discard_buffer,
                                        MSM261_DISCARD_BLOCK_SAMPLES,
                                        &samples_read, config->timeout_ms);
            if (error != ESP_OK ||
                samples_read != MSM261_DISCARD_BLOCK_SAMPLES) {
                break;
            }
            for (index = 0U; index < samples_read; ++index) {
                if (discard_buffer[index] < minimum) minimum = discard_buffer[index];
                if (discard_buffer[index] > maximum) maximum = discard_buffer[index];
            }
            if (minimum == maximum) {
                break;
            }
            valid_blocks++;
        }
        if (valid_blocks == MSM261_SLOT_VALIDATE_BLOCKS) {
            break;
        }

        MSM261_LOG_WRN("probe invalid, clk_gpio=%d data_gpio=%d slot=%s valid_blocks=%u min=%d max=%d",
                       bsp_config.clk_gpio, bsp_config.data_gpio,
                       selected_slot == MSM261_SLOT_SELECT_HIGH ? "HIGH" : "LOW",
                       (unsigned)valid_blocks, minimum, maximum);
        (void)bsp_i2s_pdm_rx_deinit();
    }
    if (slot_attempt == slot_attempt_count) {
        s_context_in_use = false;
        MSM261_LOG_ERR("init FAIL, reason=stuck_data_check_power_clock_data");
        return error == ESP_ERR_TIMEOUT ? ERR_MSM261_READ_TIMEOUT
                                        : ERR_MSM261_STUCK_DATA;
    }
    s_context.config.slot = selected_slot;

    s_context.initialized = true;
    *handle = &s_context;
    MSM261_LOG_INF("init OK, rate=%uHz bits=16 window=%ums clk_gpio=%d data_gpio=%d slot=%s",
                   MSM261_SAMPLE_RATE_HZ, MSM261_WINDOW_MS,
                   bsp_config.clk_gpio, bsp_config.data_gpio,
                   selected_slot == MSM261_SLOT_SELECT_HIGH ? "HIGH" : "LOW");
    return 0;
}

int msm261_read_level(msm261_handle_t handle, msm261_level_t *level)
{
    int64_t sum = 0;
    uint64_t sum_squares = 0U;
    int32_t mean;
    int32_t peak = 0;
    int16_t minimum = INT16_MAX;
    int16_t maximum = INT16_MIN;
    size_t samples_read = 0U;
    uint32_t clipped = 0U;
    size_t index;
    esp_err_t error;
    float rms;

    if (handle == NULL || level == NULL || handle != &s_context ||
        !handle->initialized) {
        return ERR_NOT_INIT;
    }
    memset(level, 0, sizeof(*level));
    error = bsp_i2s_pdm_rx_read(handle->samples, MSM261_WINDOW_SAMPLES,
                                &samples_read, handle->config.timeout_ms);
    if (error != ESP_OK) {
        handle->error_count++;
        level->error_count = handle->error_count;
        return msm261_map_esp_error(error);
    }
    if (samples_read != MSM261_WINDOW_SAMPLES) {
        handle->error_count++;
        level->error_count = handle->error_count;
        return ERR_MSM261_SHORT_READ;
    }

    for (index = 0U; index < samples_read; ++index) {
        int16_t sample = handle->samples[index];
        sum += sample;
        if (sample < minimum) minimum = sample;
        if (sample > maximum) maximum = sample;
        if (sample <= -MSM261_CLIP_THRESHOLD ||
            sample >= MSM261_CLIP_THRESHOLD) clipped++;
    }
    mean = (int32_t)(sum / (int64_t)samples_read);
    for (index = 0U; index < samples_read; ++index) {
        int32_t centered = (int32_t)handle->samples[index] - mean;
        int32_t magnitude = centered < 0 ? -centered : centered;
        sum_squares += (uint64_t)((int64_t)centered * centered);
        if (magnitude > peak) peak = magnitude;
    }

    handle->stuck_block_count = minimum == maximum
                                    ? handle->stuck_block_count + 1U : 0U;
    if (handle->stuck_block_count >= MSM261_STUCK_BLOCK_LIMIT) {
        handle->error_count++;
        level->error_count = handle->error_count;
        return ERR_MSM261_STUCK_DATA;
    }

    rms = sqrtf((float)sum_squares / (float)samples_read);
    handle->ok_count++;
    level->peak = (int16_t)(peak > INT16_MAX ? INT16_MAX : peak);
    level->rms = rms;
    level->dbfs = rms > 0.0F
                      ? 20.0F * log10f(rms / 32768.0F)
                      : MSM261_DBFS_FLOOR;
    if (level->dbfs < MSM261_DBFS_FLOOR) level->dbfs = MSM261_DBFS_FLOOR;
    level->dc_mean = (int16_t)mean;
    level->sample_count = (uint32_t)samples_read;
    level->clipped_samples = clipped;
    level->ok_count = handle->ok_count;
    level->error_count = handle->error_count;
    level->valid = true;
    return 0;
}

int msm261_deinit(msm261_handle_t handle)
{
    esp_err_t error;

    if (handle == NULL || handle != &s_context || !handle->initialized) {
        return ERR_NOT_INIT;
    }
    error = bsp_i2s_pdm_rx_deinit();
    memset(&s_context, 0, sizeof(s_context));
    s_context_in_use = false;
    if (error != ESP_OK) {
        MSM261_LOG_ERR("deinit FAIL, esp_err=0x%X", (unsigned)error);
        return msm261_map_esp_error(error);
    }
    MSM261_LOG_INF("deinit OK");
    return 0;
}
