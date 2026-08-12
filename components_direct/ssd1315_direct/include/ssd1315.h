/**
 * @file    ssd1315.h
 * @brief   SSD1315 OLED 直连/CH32 桥接驱动公共接口
 */

#ifndef SSD1315_H__
#define SSD1315_H__

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

#define SSD1315_I2C_ADDR                    0x3CU
#define SSD1315_I2C_FREQ_HZ                 400000U
#define SSD1315_I2C_TIMEOUT_MS              1000U
#define SSD1315_BRIDGE_TIMEOUT_MS           2000U

#define SSD1315_CH32_I2C_NODE_MIN           0x01U
#define SSD1315_CH32_I2C_NODE_MAX           0x20U
#define SSD1315_CAN_STATUS_ID_BASE          0x100U
#define SSD1315_CAN_COMMAND_ID_BASE         0x200U
#define SSD1315_CAN_ACK_ID_BASE             0x500U
#define SSD1315_CAN_HELLO_ID_BASE           0x700U

#define SSD1315_CMD_WRITE_REG               0x03U
#define SSD1315_CMD_SET_SPEED               0x07U
#define SSD1315_CMD_WRITE_MULTI             0x08U
#define SSD1315_I2C_SPEED_400K              0x01U
#define SSD1315_DEVICE_TYPE_I2C             0x01U
#define SSD1315_MULTI_FLAG_START            0x01U
#define SSD1315_MULTI_FLAG_END              0x02U
#define SSD1315_MULTI_CHUNK_SIZE             4U

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

#define PIN_SSD1315_SDA                     21
#define PIN_SSD1315_SCL                     22
#define PIN_SSD1315_RST                     (-1)

typedef struct {
    /* 公共 I2C 参数。 */
    uint8_t i2c_addr;
    uint32_t clk_speed_hz;

    /* 直连模式：BSP 未初始化时允许驱动初始化共享 I2C 总线。 */
    bool initialize_i2c;

    /* 桥接模式：node_id 由设备发现层动态传入。 */
    uint8_t ch32_node_id;
    uint32_t bridge_timeout_ms;
    bool initialize_twai;
} ssd1315_cfg_t;

typedef struct ssd1315_ctx *ssd1315_handle_t;

int ssd1315_init_device(ssd1315_handle_t *handle, const ssd1315_cfg_t *cfg);
int ssd1315_deinit_device(ssd1315_handle_t handle);
int ssd1315_clear_display(ssd1315_handle_t handle);
int ssd1315_draw_text(ssd1315_handle_t handle, uint8_t x, uint8_t page,
                      const char *text);
int ssd1315_refresh_display(ssd1315_handle_t handle);

#endif /* SSD1315_H__ */

