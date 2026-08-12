#ifndef SSD1315_H__
#define SSD1315_H__

#include <stdint.h>

#include "ch32_i2c_multi_gateway_final.h"

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

#define SSD1315_I2C_ADDR                    0x3CU
#define SSD1315_I2C_FREQ_HZ                 400000U
#define SSD1315_I2C_TIMEOUT_MS              1000U
#define SSD1315_BRIDGE_TIMEOUT_MS           2000U
#define SSD1315_CH32_I2C_NODE_MIN           0x01U
#define SSD1315_CH32_I2C_NODE_MAX           0x20U
#define SSD1315_WIDTH                       128U
#define SSD1315_HEIGHT                      64U
#define SSD1315_PAGE_COUNT                  8U
#define SSD1315_PAGE_SIZE                   128U
#define SSD1315_FRAMEBUFFER_SIZE            1024U
#define SSD1315_FONT_WIDTH                  5U
#define SSD1315_FONT_ADVANCE                6U
#define SSD1315_STARTUP_MS                  100U
#define SSD1315_MAX_RETRIES                 3U
#define SSD1315_BRIDGE_REFRESH_INTERVAL_MS  100U
#define SSD1315_MAX_BRIDGE_INSTANCES        6U

typedef struct {
    uint8_t i2c_addr;
    uint32_t clk_speed_hz;

    /*
     * Stable node-table entry owned by the discovery/device layer. It is
     * merged by token and updated in place when the runtime node_id changes.
     */
    ch32_i2c_multi_node_t *ch32_node;
    uint32_t bridge_timeout_ms;
} ssd1315_cfg_t;

typedef struct ssd1315_ctx *ssd1315_handle_t;

int ssd1315_init_device(ssd1315_handle_t *handle, const ssd1315_cfg_t *cfg);
int ssd1315_deinit_device(ssd1315_handle_t handle);
int ssd1315_clear_display(ssd1315_handle_t handle);
int ssd1315_draw_text(ssd1315_handle_t handle, uint8_t x, uint8_t page,
                      const char *text);
int ssd1315_refresh_display(ssd1315_handle_t handle);

#endif /* SSD1315_H__ */
