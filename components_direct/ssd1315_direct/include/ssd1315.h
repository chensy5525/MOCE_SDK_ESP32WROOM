/**
 * @file    ssd1315.h
 * @brief   SSD1315 OLED 直连驱动公共接口
 */

#ifndef SSD1315_H__
#define SSD1315_H__

#include <stdint.h>
#include "module_errors.h"

#define SSD1315_I2C_ADDR                    0x3CU
#define SSD1315_I2C_FREQ_HZ                 400000U
#define SSD1315_I2C_TIMEOUT_MS              1000U
#define SSD1315_REFRESH_TIMEOUT_MS          3000U
#define SSD1315_WRITE_MAX_ATTEMPTS          3U
#define SSD1315_RETRY_DELAY_MS              1U

#define SSD1315_WIDTH                       128U
#define SSD1315_HEIGHT                      64U
#define SSD1315_PAGE_COUNT                  8U
#define SSD1315_PAGE_SIZE                   128U
#define SSD1315_FRAMEBUFFER_SIZE            1024U
#define SSD1315_FONT_WIDTH                  5U
#define SSD1315_FONT_ADVANCE                6U

#define SSD1315_STARTUP_MS                  100U

typedef struct {
    uint8_t i2c_addr;
    uint32_t clk_speed_hz;
} ssd1315_cfg_t;

typedef struct ssd1315_ctx *ssd1315_handle_t;

int ssd1315_init_device(ssd1315_handle_t *handle, const ssd1315_cfg_t *cfg);
int ssd1315_deinit_device(ssd1315_handle_t handle);
int ssd1315_clear_display(ssd1315_handle_t handle);
int ssd1315_draw_text(ssd1315_handle_t handle, uint8_t x, uint8_t page,
                      const char *text);
int ssd1315_refresh_display(ssd1315_handle_t handle);

#endif /* SSD1315_H__ */
