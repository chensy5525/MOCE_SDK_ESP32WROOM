# Repository Migration Checklist

Use this checklist when copying a verified module package into another ESP32
repository with the same base SDK structure.  Copy complete component/example
directories; do not copy build output or historical staging directories.

## I2C CH32 Bridge Package

Copy:

```
components_esp32wroom/ch32_can_gateway_core/
components_esp32wroom/ch32_i2c_multi_gateway_final/
components_ch32/ch32_<module>_gateway/
examples_ch32/<module>_ch32_test/
```

Dependency direction:

```
example -> module bridge driver
        -> ch32_i2c_multi_gateway_final
        -> ch32_can_gateway_core
```

## UART CH32 Bridge Package

Copy:

```
components_esp32wroom/ch32_can_gateway_core/
components_esp32wroom/ch32_uart_dynamic_gateway_final/
components_ch32/ch32_<module>_gateway/
examples_ch32/<module>_ch32_test/
```

Dependency direction:

```
example -> module bridge driver
        -> ch32_uart_dynamic_gateway_final
        -> ch32_can_gateway_core
```

`ch32_uart_dynamic_gateway_final` must not depend on
`ch32_i2c_multi_gateway_final`.

## Direct Package

Copy:

```
components_direct/<module>_direct/
examples_direct/<module>_direct_test/
```

The destination repository must already contain the BSP and board components
named by the driver's `CMakeLists.txt`.

## Destination Checks

1. Check the example's `EXTRA_COMPONENT_DIRS` and every component's `REQUIRES`.
2. Confirm `BOARD_CAN_TX_GPIO` and `BOARD_CAN_RX_GPIO` in the destination
   board profile match the hardware.
3. Confirm every ESP32/CH32 node uses the same CAN bitrate (currently
   500 kbit/s for the verified gateway set).
4. Confirm the matching CH32 gateway firmware is flashed.
5. Build from a clean/new build directory.
6. Flash the ESP32 and capture UART0 logs.
7. For bridge packages, verify F0 discovery, F1 assignment and matching F2
   confirmation before interpreting downstream-module errors.
8. Verify downstream scan/probe, module init and the documented observable
   behavior as separate stages.

## Do Not Copy

- `build/`, `.bin`, `.elf`, object files or generated sdkconfig output unless
  the release process explicitly requires an artifact.
- Directories named `.tmp*`, `_tmp*`, `tmp`, `staging*`, `revision*`,
  `build*` or `sdk_output` from the knowledge base.
- Backup folders such as `ch32_i2c_multi_gateway_final - 副本` or
  `ch32_uart_dynamic_gateway_final - 副本`.

Formal code remains in the ESP32/CH32 repositories.  The knowledge base records
the verified path and result rather than maintaining another source-code copy.
