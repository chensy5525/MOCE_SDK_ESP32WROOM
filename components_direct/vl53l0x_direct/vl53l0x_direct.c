#include "vl53l0x.h"
#include <stdio.h>
#include <string.h>
#include "bsp_i2c.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#define TAG "VL53L0X"
#define SYSRANGE_START 0x00U
#define SEQUENCE_CONFIG 0x01U
#define INTERRUPT_CONFIG 0x0AU
#define INTERRUPT_CLEAR 0x0BU
#define INTERRUPT_STATUS 0x13U
#define RANGE_STATUS 0x14U
#define RATE_LIMIT 0x44U
#define MSRC_CONTROL 0x60U
#define GPIO_MUX 0x84U
#define EXTSUP_HV 0x89U
#define SPAD_MAP 0xB0U
#define MODEL_ID_REG 0xC0U
#define POLL_MS 10U
#define LOGI(f,...) printf("[INF][" TAG "] " f "\n",##__VA_ARGS__)
#define LOGE(f,...) printf("[ERR][" TAG "] " f "\n",##__VA_ARGS__)
#define TRY(x) do { int r_=(x); if(r_!=0) return r_; } while(0)
typedef struct vl53l0x_ctx { bool allocated,initialized; i2c_master_dev_handle_t dev; vl53l0x_cfg_t cfg; uint8_t stop; } vl53l0x_ctx_t;
static vl53l0x_ctx_t s;
static void delay_ms(uint32_t ms){ TickType_t t=pdMS_TO_TICKS(ms); vTaskDelay(t?t:1U); }
static int map_err(esp_err_t e){ return e==ESP_ERR_TIMEOUT?ERR_TIMEOUT:(e==ESP_ERR_INVALID_ARG?ERR_INVALID_PARAM:ERR_VL53L0X_COMM); }
static int wr(vl53l0x_ctx_t*c,const uint8_t*d,size_t n){ esp_err_t e=ESP_FAIL; for(unsigned i=0;i<VL53L0X_MAX_RETRIES;i++){e=bsp_i2c_write(c->dev,d,n,(int)c->cfg.timeout_ms);if(e==ESP_OK)return 0;delay_ms(2);}LOGE("i2c write FAIL err=%s",esp_err_to_name(e));return map_err(e);}
static int rd(vl53l0x_ctx_t*c,uint8_t reg,uint8_t*d,size_t n){esp_err_t e=ESP_FAIL;for(unsigned i=0;i<VL53L0X_MAX_RETRIES;i++){e=bsp_i2c_write_read(c->dev,&reg,1,d,n,(int)c->cfg.timeout_ms);if(e==ESP_OK)return 0;delay_ms(2);}LOGE("i2c read FAIL reg=0x%02X err=%s",reg,esp_err_to_name(e));return map_err(e);}
static int w8(vl53l0x_ctx_t*c,uint8_t r,uint8_t v){uint8_t d[2]={r,v};return wr(c,d,2);}
static int w16(vl53l0x_ctx_t*c,uint8_t r,uint16_t v){uint8_t d[3]={r,(uint8_t)(v>>8),(uint8_t)v};return wr(c,d,3);}
static int r8(vl53l0x_ctx_t*c,uint8_t r,uint8_t*v){return rd(c,r,v,1);}
static int upd(vl53l0x_ctx_t*c,uint8_t r,uint8_t set,uint8_t clr){uint8_t v;TRY(r8(c,r,&v));return w8(c,r,(v|set)&~clr);}
static int wait_mask(vl53l0x_ctx_t*c,uint8_t r,uint8_t mask,uint32_t timeout){uint8_t v;for(uint32_t e=0;e<timeout;e+=POLL_MS){TRY(r8(c,r,&v));if(v&mask)return 0;delay_ms(POLL_MS);}return ERR_VL53L0X_DATA_NOT_READY;}
static int spad_info(vl53l0x_ctx_t*c,uint8_t*count,bool*ap){uint8_t v;TRY(w8(c,0x80,1));TRY(w8(c,0xFF,1));TRY(w8(c,0,0));TRY(w8(c,0xFF,6));TRY(upd(c,0x83,4,0));TRY(w8(c,0xFF,7));TRY(w8(c,0x81,1));TRY(w8(c,0x80,1));TRY(w8(c,0x94,0x6B));TRY(w8(c,0x83,0));TRY(wait_mask(c,0x83,0xFF,1000));TRY(w8(c,0x83,1));TRY(r8(c,0x92,&v));*count=v&0x7F;*ap=(v&0x80)!=0;TRY(w8(c,0x81,0));TRY(w8(c,0xFF,6));TRY(upd(c,0x83,0,4));TRY(w8(c,0xFF,1));TRY(w8(c,0,1));TRY(w8(c,0xFF,0));return w8(c,0x80,0);}
static int tuning(vl53l0x_ctx_t*c){static const uint8_t a[][2]={{0xFF,1},{0,0},{0xFF,0},{9,0},{0x10,0},{0x11,0},{0x24,1},{0x25,0xFF},{0x75,0},{0xFF,1},{0x4E,0x2C},{0x48,0},{0x30,0x20},{0xFF,0},{0x30,9},{0x54,0},{0x31,4},{0x32,3},{0x40,0x83},{0x46,0x25},{0x60,0},{0x27,0},{0x50,6},{0x51,0},{0x52,0x96},{0x56,8},{0x57,0x30},{0x61,0},{0x62,0},{0x64,0},{0x65,0},{0x66,0xA0},{0xFF,1},{0x22,0x32},{0x47,0x14},{0x49,0xFF},{0x4A,0},{0xFF,0},{0x7A,0x0A},{0x7B,0},{0x78,0x21},{0xFF,1},{0x23,0x34},{0x42,0},{0x44,0xFF},{0x45,0x26},{0x46,5},{0x40,0x40},{0x0E,6},{0x20,0x1A},{0x43,0x40},{0xFF,0},{0x34,3},{0x35,0x44},{0xFF,1},{0x31,4},{0x4B,9},{0x4C,5},{0x4D,4},{0xFF,0},{0x44,0},{0x45,0x20},{0x47,8},{0x48,0x28},{0x67,0},{0x70,4},{0x71,1},{0x72,0xFE},{0x76,0},{0x77,0},{0xFF,1},{0x0D,1},{0xFF,0},{0x80,1},{1,0xF8},{0xFF,1},{0x8E,1},{0,1},{0xFF,0},{0x80,0}};for(size_t i=0;i<sizeof(a)/sizeof(a[0]);i++)TRY(w8(c,a[i][0],a[i][1]));return 0;}
static int calibrate(vl53l0x_ctx_t*c,uint8_t vhv){TRY(w8(c,SYSRANGE_START,1|vhv));TRY(wait_mask(c,INTERRUPT_STATUS,7,1000));TRY(w8(c,INTERRUPT_CLEAR,1));return w8(c,SYSRANGE_START,0);}
static int sensor_init(vl53l0x_ctx_t*c){uint8_t id,v,count,en=0,map[6];bool ap;TRY(r8(c,MODEL_ID_REG,&id));if(id!=VL53L0X_MODEL_ID){LOGE("id_check FAIL expected=0x%02X got=0x%02X",VL53L0X_MODEL_ID,id);return ERR_VL53L0X_ID_MISMATCH;}TRY(upd(c,EXTSUP_HV,1,0));TRY(w8(c,0x88,0));TRY(w8(c,0x80,1));TRY(w8(c,0xFF,1));TRY(w8(c,0,0));TRY(r8(c,0x91,&c->stop));TRY(w8(c,0,1));TRY(w8(c,0xFF,0));TRY(w8(c,0x80,0));TRY(upd(c,MSRC_CONTROL,0x12,0));TRY(w16(c,RATE_LIMIT,32));TRY(w8(c,SEQUENCE_CONFIG,0xFF));TRY(spad_info(c,&count,&ap));TRY(rd(c,SPAD_MAP,map,6));TRY(w8(c,0xFF,1));TRY(w8(c,0x4F,0));TRY(w8(c,0x4E,0x2C));TRY(w8(c,0xFF,0));TRY(w8(c,0xB6,0xB4));for(uint8_t i=0;i<48;i++){if(i<(ap?12:0)||en==count)map[i/8]&=(uint8_t)~(1U<<(i%8));else if((map[i/8]>>(i%8))&1)en++;}{uint8_t d[7]={SPAD_MAP};memcpy(d+1,map,6);TRY(wr(c,d,7));}TRY(tuning(c));TRY(w8(c,INTERRUPT_CONFIG,4));TRY(r8(c,GPIO_MUX,&v));TRY(w8(c,GPIO_MUX,v&~0x10));TRY(w8(c,INTERRUPT_CLEAR,1));TRY(w8(c,SEQUENCE_CONFIG,0xE8));TRY(w8(c,SEQUENCE_CONFIG,1));TRY(calibrate(c,0x40));TRY(w8(c,SEQUENCE_CONFIG,2));TRY(calibrate(c,0));TRY(w8(c,SEQUENCE_CONFIG,0xE8));TRY(w8(c,0x80,1));TRY(w8(c,0xFF,1));TRY(w8(c,0,0));TRY(w8(c,0x91,c->stop));TRY(w8(c,0,1));TRY(w8(c,0xFF,0));TRY(w8(c,0x80,0));return w8(c,SYSRANGE_START,2);}
int vl53l0x_init(vl53l0x_handle_t*h,const vl53l0x_cfg_t*c){if(!h||!c||s.allocated||c->i2c_addr!=0x29||!c->bus_speed_hz||c->bus_speed_hz>400000||!c->timeout_ms||!c->data_ready_timeout_ms)return ERR_INVALID_PARAM;*h=NULL;memset(&s,0,sizeof(s));s.allocated=true;s.cfg=*c;esp_err_t e=ESP_OK;if(c->initialize_i2c||!bsp_i2c_is_initialized())e=bsp_i2c_init();if(e==ESP_OK)e=bsp_i2c_add_device_7bit(c->i2c_addr,c->bus_speed_hz,&s.dev);if(e!=ESP_OK){memset(&s,0,sizeof(s));return map_err(e);}int r=sensor_init(&s);if(r){bsp_i2c_remove_device(s.dev);memset(&s,0,sizeof(s));LOGE("init FAIL err=%d",r);return r;}s.initialized=true;*h=&s;LOGI("init OK addr=0x29 id=0xEE");return 0;}
int vl53l0x_deinit(vl53l0x_handle_t h){if(h!=&s||!s.initialized)return ERR_NOT_INIT;int r=w8(&s,SYSRANGE_START,1);bsp_i2c_remove_device(s.dev);memset(&s,0,sizeof(s));return r;}
int vl53l0x_read(vl53l0x_handle_t h,vl53l0x_result_t*out){uint8_t d[12];if(h!=&s||!out)return ERR_INVALID_PARAM;if(!s.initialized)return ERR_NOT_INIT;memset(out,0,sizeof(*out));TRY(wait_mask(&s,INTERRUPT_STATUS,7,s.cfg.data_ready_timeout_ms));TRY(rd(&s,RANGE_STATUS,d,12));TRY(w8(&s,INTERRUPT_CLEAR,1));out->range_status=(d[0]&0x78)>>3;out->distance_mm=((uint16_t)d[10]<<8)|d[11];out->valid=out->distance_mm<=VL53L0X_RANGE_MAX_MM;out->out_of_range=!out->valid;return out->valid?0:ERR_VL53L0X_OUT_OF_RANGE;}
