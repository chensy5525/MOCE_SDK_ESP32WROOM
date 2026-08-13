#include "vl53l4cd.h"
#include <stdio.h>
#include <string.h>
#include "bsp_i2c.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#define TAG "VL53L4CD"
#define REG_OSC_FREQUENCY 0x0006U
#define REG_VHV_TIMEOUT 0x0008U
#define REG_GPIO_MUX 0x0030U
#define REG_GPIO_STATUS 0x0031U
#define REG_RANGE_CONFIG_A 0x005EU
#define REG_RANGE_CONFIG_B 0x0061U
#define REG_INTERMEASUREMENT 0x006CU
#define REG_INTERRUPT_CLEAR 0x0086U
#define REG_SYSTEM_START 0x0087U
#define REG_RANGE_STATUS 0x0089U
#define REG_DISTANCE 0x0096U
#define REG_FIRMWARE_STATUS 0x00E5U
#define REG_MODEL_ID 0x010FU
#define POLL_MS 5U
#define LOG_INF(f, ...) printf("[INF][" TAG "] " f "\n", ##__VA_ARGS__)
#define LOG_ERR(f, ...) printf("[ERR][" TAG "] " f "\n", ##__VA_ARGS__)
#define TRY(x) do { int r_=(x); if(r_!=0)return r_; } while(0)
typedef struct vl53l4cd_ctx { bool allocated,initialized; i2c_master_dev_handle_t dev; vl53l4cd_cfg_t cfg; } vl53l4cd_ctx_t;
static vl53l4cd_ctx_t s_instance;
/* ST VL53L4CD ULD V1.0.0 default configuration, registers 0x002D..0x0087. */
static const uint8_t s_default_config[] = {
0x12,0x00,0x00,0x11,0x02,0x00,0x02,0x08,0x00,0x08,0x10,0x01,0x01,0x00,0x00,0x00,
0x00,0xFF,0x00,0x0F,0x00,0x00,0x00,0x00,0x00,0x20,0x0B,0x00,0x00,0x02,0x14,0x21,
0x00,0x00,0x05,0x00,0x00,0x00,0x00,0xC8,0x00,0x00,0x38,0xFF,0x01,0x00,0x08,0x00,
0x00,0x01,0xCC,0x07,0x01,0xF1,0x05,0x00,0xA0,0x00,0x80,0x08,0x38,0x00,0x00,0x00,
0x00,0x0F,0x89,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x01,0x07,0x05,0x06,0x06,0x00,
0x00,0x02,0xC7,0xFF,0x9B,0x00,0x00,0x00,0x01,0x00,0x00};
static void delay_ms(uint32_t ms){TickType_t t=pdMS_TO_TICKS(ms);vTaskDelay(t?t:1U);}
static int map_err(esp_err_t e){return e==ESP_ERR_TIMEOUT?ERR_TIMEOUT:(e==ESP_ERR_INVALID_ARG?ERR_INVALID_PARAM:ERR_VL53L4CD_COMM);}
static int write_data(vl53l4cd_ctx_t*c,uint16_t reg,const uint8_t*data,size_t len){uint8_t b[32];while(len){size_t n=len>30U?30U:len;b[0]=(uint8_t)(reg>>8);b[1]=(uint8_t)reg;memcpy(b+2,data,n);esp_err_t e=ESP_FAIL;for(uint32_t i=0;i<VL53L4CD_MAX_RETRIES;i++){e=bsp_i2c_write(c->dev,b,n+2U,(int)c->cfg.timeout_ms);if(e==ESP_OK)break;delay_ms(2U);}if(e!=ESP_OK){LOG_ERR("i2c write FAIL reg=0x%04X err=%s",reg,esp_err_to_name(e));return map_err(e);}reg=(uint16_t)(reg+n);data+=n;len-=n;}return 0;}
static int read_data(vl53l4cd_ctx_t*c,uint16_t reg,uint8_t*data,size_t len){uint8_t idx[2]={(uint8_t)(reg>>8),(uint8_t)reg};esp_err_t e=ESP_FAIL;for(uint32_t i=0;i<VL53L4CD_MAX_RETRIES;i++){e=bsp_i2c_write_read(c->dev,idx,2U,data,len,(int)c->cfg.timeout_ms);if(e==ESP_OK)return 0;delay_ms(2U);}LOG_ERR("i2c read FAIL reg=0x%04X err=%s",reg,esp_err_to_name(e));return map_err(e);}
static int w8(vl53l4cd_ctx_t*c,uint16_t r,uint8_t v){return write_data(c,r,&v,1U);}static int w16(vl53l4cd_ctx_t*c,uint16_t r,uint16_t v){uint8_t d[2]={(uint8_t)(v>>8),(uint8_t)v};return write_data(c,r,d,2U);}static int w32(vl53l4cd_ctx_t*c,uint16_t r,uint32_t v){uint8_t d[4]={(uint8_t)(v>>24),(uint8_t)(v>>16),(uint8_t)(v>>8),(uint8_t)v};return write_data(c,r,d,4U);}static int r8(vl53l4cd_ctx_t*c,uint16_t r,uint8_t*v){return read_data(c,r,v,1U);}static int r16(vl53l4cd_ctx_t*c,uint16_t r,uint16_t*v){uint8_t d[2];TRY(read_data(c,r,d,2U));*v=((uint16_t)d[0]<<8)|d[1];return 0;}
static int data_ready(vl53l4cd_ctx_t*c,bool*ready){uint8_t mux,status;TRY(r8(c,REG_GPIO_MUX,&mux));TRY(r8(c,REG_GPIO_STATUS,&status));*ready=(status&1U)==(((mux>>4U)&1U)?0U:1U);return 0;}
static int wait_ready(vl53l4cd_ctx_t*c,uint32_t timeout){for(uint32_t e=0;e<timeout;e+=POLL_MS){bool ready;TRY(data_ready(c,&ready));if(ready)return 0;delay_ms(POLL_MS);}return ERR_VL53L4CD_DATA_NOT_READY;}
static int set_range_timing(vl53l4cd_ctx_t*c,uint32_t ms){uint16_t osc,enc,exp=0;uint32_t budget,macro,val,tmp;TRY(r16(c,REG_OSC_FREQUENCY,&osc));if(!osc||ms<10U||ms>200U)return ERR_INVALID_PARAM;budget=ms*1000U-2500U;macro=(uint32_t)(((uint64_t)2304U*(0x40000000ULL/osc))>>6U);budget<<=12U;tmp=macro*16U;val=((budget+((tmp>>6U)>>1U))/(tmp>>6U))-1U;while(val&0xFFFFFF00U){val>>=1U;exp++;}enc=(uint16_t)((exp<<8U)|(val&0xFFU));TRY(w16(c,REG_RANGE_CONFIG_A,enc));exp=0;tmp=macro*12U;val=((budget+((tmp>>6U)>>1U))/(tmp>>6U))-1U;while(val&0xFFFFFF00U){val>>=1U;exp++;}return w16(c,REG_RANGE_CONFIG_B,(uint16_t)((exp<<8U)|(val&0xFFU)));}
static int sensor_init(vl53l4cd_ctx_t*c){uint16_t id;uint8_t fw;TRY(r16(c,REG_MODEL_ID,&id));if(id!=VL53L4CD_MODEL_ID){LOG_ERR("id_check FAIL expected=0x%04X got=0x%04X",VL53L4CD_MODEL_ID,id);return ERR_VL53L4CD_ID_MISMATCH;}for(uint32_t i=0;i<1000U;i++){TRY(r8(c,REG_FIRMWARE_STATUS,&fw));if(fw==3U)break;if(i==999U)return ERR_TIMEOUT;delay_ms(1U);}TRY(write_data(c,0x002DU,s_default_config,sizeof(s_default_config)));TRY(w8(c,REG_SYSTEM_START,0x40U));TRY(wait_ready(c,1000U));TRY(w8(c,REG_INTERRUPT_CLEAR,1U));TRY(w8(c,REG_SYSTEM_START,0U));TRY(w8(c,REG_VHV_TIMEOUT,0x09U));TRY(w8(c,0x000BU,0U));TRY(w16(c,0x0024U,0x0500U));TRY(set_range_timing(c,50U));TRY(w32(c,REG_INTERMEASUREMENT,0U));TRY(w8(c,REG_SYSTEM_START,0x21U));TRY(wait_ready(c,1000U));return w8(c,REG_INTERRUPT_CLEAR,1U);}
int vl53l4cd_init(vl53l4cd_handle_t*h,const vl53l4cd_cfg_t*cfg){if(!h||!cfg||s_instance.allocated||cfg->i2c_addr!=0x29U||!cfg->bus_speed_hz||cfg->bus_speed_hz>400000U||!cfg->timeout_ms||!cfg->data_ready_timeout_ms)return ERR_INVALID_PARAM;*h=NULL;memset(&s_instance,0,sizeof(s_instance));s_instance.allocated=true;s_instance.cfg=*cfg;esp_err_t e=ESP_OK;if(cfg->initialize_i2c||!bsp_i2c_is_initialized())e=bsp_i2c_init();if(e==ESP_OK)e=bsp_i2c_add_device_7bit(cfg->i2c_addr,cfg->bus_speed_hz,&s_instance.dev);if(e!=ESP_OK){memset(&s_instance,0,sizeof(s_instance));return map_err(e);}int r=sensor_init(&s_instance);if(r){bsp_i2c_remove_device(s_instance.dev);memset(&s_instance,0,sizeof(s_instance));LOG_ERR("init FAIL err=%d",r);return r;}s_instance.initialized=true;*h=&s_instance;LOG_INF("init OK addr=0x29 id=0xEBAA range_max=1300mm");return 0;}
int vl53l4cd_deinit(vl53l4cd_handle_t h){if(h!=&s_instance||!s_instance.initialized)return ERR_NOT_INIT;int r=w8(&s_instance,REG_SYSTEM_START,0U);bsp_i2c_remove_device(s_instance.dev);memset(&s_instance,0,sizeof(s_instance));LOG_INF("deinit %s",r==0?"OK":"FAIL");return r;}
int vl53l4cd_read(vl53l4cd_handle_t h,vl53l4cd_result_t*out){static const uint8_t map[24]={255,255,255,5,2,4,1,7,3,0,255,255,9,13,255,255,255,255,10,6,255,255,11,12};uint8_t raw;uint16_t distance;if(h!=&s_instance||!out)return ERR_INVALID_PARAM;if(!s_instance.initialized)return ERR_NOT_INIT;memset(out,0,sizeof(*out));TRY(wait_ready(&s_instance,s_instance.cfg.data_ready_timeout_ms));TRY(r8(&s_instance,REG_RANGE_STATUS,&raw));TRY(r16(&s_instance,REG_DISTANCE,&distance));TRY(w8(&s_instance,REG_INTERRUPT_CLEAR,1U));raw&=0x1FU;out->range_status=raw<24U?map[raw]:255U;out->distance_mm=distance;out->valid=out->range_status==0U&&distance<=VL53L4CD_RANGE_MAX_MM;out->out_of_range=!out->valid;return out->valid?0:ERR_VL53L4CD_OUT_OF_RANGE;}
