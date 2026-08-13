#include "vl53l4cd.h"
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#define TAG "VL53L4CD_EXAMPLE"
void app_main(void){const vl53l4cd_cfg_t cfg={.i2c_addr=VL53L4CD_I2C_ADDR_DEFAULT,.bus_speed_hz=VL53L4CD_BUS_SPEED_HZ,.timeout_ms=VL53L4CD_I2C_TIMEOUT_MS,.data_ready_timeout_ms=VL53L4CD_DATA_READY_TIMEOUT_MS,.initialize_i2c=true};vl53l4cd_handle_t sensor=NULL;vl53l4cd_result_t data;int r=vl53l4cd_init(&sensor,&cfg);if(r){printf("[ERR][" TAG "] init FAIL err=%d\n",r);return;}for(uint32_t i=0;i<50U;i++){r=vl53l4cd_read(sensor,&data);if(r==0)printf("[INF][" TAG "] data valid=1 mm=%u status=%u\n",data.distance_mm,data.range_status);else if(r==ERR_VL53L4CD_OUT_OF_RANGE)printf("[WRN][" TAG "] OUT RANGE mm=%u status=%u\n",data.distance_mm,data.range_status);else printf("[ERR][" TAG "] read FAIL err=%d\n",r);vTaskDelay(pdMS_TO_TICKS(200U));}r=vl53l4cd_deinit(sensor);if(r)printf("[ERR][" TAG "] deinit FAIL err=%d\n",r);}
