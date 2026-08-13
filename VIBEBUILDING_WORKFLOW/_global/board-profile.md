# ESP32-WROOM Board Profile

Pin map and peripheral configuration for the current project board.
AI must NOT invent GPIO numbers — always reference this file.

## Pin Map Summary

| GPIO | Function | Notes |
|------|----------|-------|
| 0 | Button | Boot button, active low, internal pull-up |
| 2 | GPIO_IO2 | Extra header |
| 4 | CAN_RX | CAN transceiver |
| 5 | CAN_TX | CAN transceiver |
| 12 | LED | On-board LED, PWM capable |
| 13 | SPI_CS1 | SPI header |
| 14 | GPIO_IO14 | Extra header |
| 15 | SPI_CS0 | SPI header |
| 16 | UART_RX | UART1 external serial |
| 17 | UART_TX | UART1 external serial |
| 18 | SPI_SCK | SPI header |
| 19 | SPI_MISO | SPI header |
| 21 | I2C_SDA | I2C0 bus, internal pull-up |
| 22 | I2C_SCL | I2C0 bus, internal pull-up |
| 23 | SPI_MOSI | SPI header |
| 25 | PWM_B3 | Output capable |
| 26 | PWM_B4 | Output capable |
| 27 | GPIO_IO27 | Extra header |
| 32 | PWM_B1 / SERVO_0 | Output capable |
| 33 | PWM_B2 / SERVO_1 | Output capable |
| 34 | PWM_A3 | INPUT-ONLY |
| 35 | PWM_A4 | INPUT-ONLY |
| 36 | PWM_A1 | INPUT-ONLY |
| 39 | PWM_A2 | INPUT-ONLY |

## Critical GPIO Constraints

- GPIO 34,35,36,39 are **input-only** — cannot be used for output, PWM, or I2C
- GPIO 21,22 are the only I2C pins (I2C_NUM_0) — all I2C devices share this bus
- GPIO 17,16 are UART1 — external serial (not programming/log serial)
- GPIO 5,4 are CAN — used exclusively for CH32 communication
- GPIO 0 is boot button — avoid using for general output during boot

## I2C Bus

```
Port:        I2C_NUM_0
SDA:         GPIO 21
SCL:         GPIO 22
Frequency:   400 kHz (fast mode)
Timeout:     1000 ms
Pull-up:     Internal (enabled)
Glitch:      7 counts ignored
Max write:   32 bytes per transaction (board limit)
OLED addr:   0x3C (fixed in project)
```

API (from bsp/bsp_i2c):

```c
esp_err_t bsp_i2c_init(void);
i2c_master_bus_handle_t bsp_i2c_get_bus_handle(void);
esp_err_t bsp_i2c_add_device_7bit(uint8_t addr, uint32_t scl_hz, i2c_master_dev_handle_t *out);
esp_err_t bsp_i2c_probe(uint8_t addr, int timeout_ms);
esp_err_t bsp_i2c_write_reg_byte(i2c_master_dev_handle_t dev, uint8_t reg, uint8_t val, int timeout_ms);
esp_err_t bsp_i2c_read_reg_byte(i2c_master_dev_handle_t dev, uint8_t reg, uint8_t *val, int timeout_ms);
esp_err_t bsp_i2c_write_reg(i2c_master_dev_handle_t dev, uint8_t reg, const uint8_t *data, size_t len, int timeout_ms);
esp_err_t bsp_i2c_read_reg(i2c_master_dev_handle_t dev, uint8_t reg, uint8_t *data, size_t len, int timeout_ms);
```

**AI must use bsp_i2c_add_device_7bit() for each I2C module's init, then use the returned dev_handle for all reads/writes.**

## UART1 (External Serial)

```
Port:      UART_NUM_1
TX:        GPIO 17
RX:        GPIO 16
Baud:      9600 (default, per-board config)
RX buffer: 256 bytes
```

API (from bsp/bsp_uart):

```c
esp_err_t bsp_uart_init(const bsp_uart_config_t *config);
esp_err_t bsp_uart_init_default(void);
esp_err_t bsp_uart_write(const void *data, size_t len, uint32_t timeout_ms);
int bsp_uart_read(void *data, size_t len, uint32_t timeout_ms);
```

**AI: UART0 is reserved for programming/log. Module drivers use UART1.**

## PWM (LEDC)

```
LEDC timers:   TIMER_0 (LED), TIMER_1 (Servo), TIMER_2 (Motor)
LED:           GPIO 12, 5kHz, 10-bit
Servo_0:       GPIO 32, 50Hz, 14-bit, 500-2500us pulse
Servo_1:       GPIO 33, 50Hz, 14-bit, 500-2500us pulse
Motor_Left:    GPIO 32 (PWM) + 33 (IN1) + 25 (IN2), 20kHz, 10-bit
Motor_Right:   GPIO 26 (PWM) + 27 (IN1) + 14 (IN2), 20kHz, 10-bit
```

API (from bsp/bsp_pwm):

```c
esp_err_t bsp_pwm_timer_init(const bsp_pwm_timer_config_t *config);
esp_err_t bsp_pwm_channel_init(const bsp_pwm_channel_config_t *config);
esp_err_t bsp_pwm_set_duty(ledc_mode_t mode, ledc_channel_t ch, uint32_t duty);
uint32_t bsp_pwm_max_duty(ledc_timer_bit_t resolution);
```

## CAN Bus

```
TX:  GPIO 5
RX:  GPIO 4
```

Used exclusively for ESP32-CH32 communication. Module drivers do NOT touch CAN
directly. `ch32_can_gateway_core` owns TWAI and frame routing; the I2C/UART
gateway protocol layers own discovery and protocol parsing.

## SPI

```
MOSI: GPIO 23
MISO: GPIO 19
SCK:  GPIO 18
CS0:  GPIO 15
CS1:  GPIO 13
```

## Available (Unused) GPIOs

```
GPIO 2, 14, 27  — Extra header pins, output capable
GPIO 34,35,36,39 — Input-only, suitable for encoder inputs
```

## Board Constants for AI

```c
// OLED fixed address in this project
#define BOARD_OLED_I2C_ADDRESS 0x3C

// I2C bus
#define BOARD_I2C_PORT        I2C_NUM_0
#define BOARD_I2C_SDA_GPIO    21
#define BOARD_I2C_SCL_GPIO    22
#define BOARD_I2C_FREQUENCY_HZ   400000
#define BOARD_I2C_TIMEOUT_MS  1000

// UART1
#define BOARD_UART_PORT       1
#define BOARD_UART_TX_GPIO    17
#define BOARD_UART_RX_GPIO    16
#define BOARD_UART_BAUD_RATE     9600

// CAN
#define BOARD_CAN_TX_GPIO     5
#define BOARD_CAN_RX_GPIO     4
```

