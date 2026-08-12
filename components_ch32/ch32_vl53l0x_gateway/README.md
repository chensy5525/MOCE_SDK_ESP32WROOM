# ch32_vl53l0x_gateway

VL53L0X 的 ESP32 侧 CH32-I2C 桥接驱动。

组件只依赖 `ch32_i2c_multi_gateway_final`，通过发现层拥有的稳定
`ch32_i2c_multi_node_t` 引用访问下游 VL53L0X。组件不安装 TWAI、不发送
F0/F1/F2、不分配节点 ID，也不长期复制运行时 `node_id`。

规范公共头为 `ch32_vl53l0x_gateway.h`。`vl53l0x.h` 仅用于兼容早期生成
代码，防止桥接头与直连组件的同名头文件在组合工程中产生歧义。
