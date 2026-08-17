#ifndef CH32_SSD1315_GATEWAY_H__
#define CH32_SSD1315_GATEWAY_H__

#include <stdint.h>

#include "ch32_i2c_multi_gateway_final.h"
#include "module_errors.h"

#define CH32_SSD1315_I2C_ADDR             0x3CU
#define CH32_SSD1315_I2C_FREQ_HZ          400000U
#define CH32_SSD1315_GATEWAY_TIMEOUT_MS   2000U
#define CH32_SSD1315_REFRESH_TIMEOUT_MS   10000U
#define CH32_SSD1315_I2C_NODE_MIN         0x01U
#define CH32_SSD1315_I2C_NODE_MAX         0x20U
#define CH32_SSD1315_WIDTH                128U
#define CH32_SSD1315_HEIGHT               64U
#define CH32_SSD1315_PAGE_COUNT           8U
#define CH32_SSD1315_PAGE_SIZE            128U
#define CH32_SSD1315_FRAMEBUFFER_SIZE     1024U
#define CH32_SSD1315_FONT_WIDTH           5U
#define CH32_SSD1315_FONT_ADVANCE         6U
#define CH32_SSD1315_STARTUP_MS           100U
#define CH32_SSD1315_MAX_INSTANCES        6U

typedef struct {
    uint8_t i2c_addr;
    uint32_t clk_speed_hz;
    ch32_i2c_multi_node_t *ch32_node;
} ch32_ssd1315_cfg_t;

typedef struct ch32_ssd1315_ctx *ch32_ssd1315_handle_t;

int ch32_ssd1315_init(ch32_ssd1315_handle_t *handle,
                      const ch32_ssd1315_cfg_t *cfg);
int ch32_ssd1315_deinit(ch32_ssd1315_handle_t handle);
int ch32_ssd1315_clear(ch32_ssd1315_handle_t handle);
int ch32_ssd1315_draw_text(ch32_ssd1315_handle_t handle, uint8_t x,
                           uint8_t page, const char *text);
int ch32_ssd1315_refresh(ch32_ssd1315_handle_t handle);

#endif /* CH32_SSD1315_GATEWAY_H__ */
