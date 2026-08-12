#include "vl53l0x.h"
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#define TAG "VL53L0X_EXAMPLE"
void app_main(void)
{
    const vl53l0x_cfg_t cfg={.i2c_addr=VL53L0X_I2C_ADDR_DEFAULT,.bus_speed_hz=VL53L0X_BUS_SPEED_HZ,.timeout_ms=VL53L0X_I2C_TIMEOUT_MS,.data_ready_timeout_ms=VL53L0X_DATA_READY_TIMEOUT_MS,.initialize_i2c=true};
    vl53l0x_handle_t sensor=NULL;
    vl53l0x_result_t data;
    int result=vl53l0x_init(&sensor,&cfg);
    if(result){printf("[ERR][" TAG "] init FAIL err=%d\n",result);return;}
    for(uint32_t i=0;i<50U;i++){
        result=vl53l0x_read(sensor,&data);
        if(result==0)printf("[INF][" TAG "] data valid=1 mm=%u status=%u\n",data.distance_mm,data.range_status);
        else if(result==ERR_VL53L0X_OUT_OF_RANGE)printf("[WRN][" TAG "] OUT RANGE mm=%u status=%u\n",data.distance_mm,data.range_status);
        else printf("[ERR][" TAG "] read FAIL err=%d\n",result);
        vTaskDelay(pdMS_TO_TICKS(200U));
    }
    result=vl53l0x_deinit(sensor);
    if(result)printf("[ERR][" TAG "] deinit FAIL err=%d\n",result);
}
