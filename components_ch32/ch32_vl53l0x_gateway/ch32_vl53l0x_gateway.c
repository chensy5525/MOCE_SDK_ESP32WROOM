#include "ch32_vl53l0x_gateway.h"
#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define TAG "VL53L0X"
#define REG_SYSRANGE_START 0x00U
#define REG_SEQUENCE_CONFIG 0x01U
#define REG_INTERRUPT_CONFIG 0x0AU
#define REG_INTERRUPT_CLEAR 0x0BU
#define REG_INTERRUPT_STATUS 0x13U
#define REG_RANGE_STATUS 0x14U
#define REG_RATE_LIMIT 0x44U
#define REG_MSRC_CONTROL 0x60U
#define REG_GPIO_MUX 0x84U
#define REG_EXTSUP_HV 0x89U
#define REG_SPAD_MAP 0xB0U
#define REG_MODEL_ID 0xC0U
#define VL53L0X_POLL_MS 10U
#define LOG_INF(f, ...) printf("[INF][" TAG "] " f "\n", ##__VA_ARGS__)
#define LOG_WRN(f, ...) printf("[WRN][" TAG "] " f "\n", ##__VA_ARGS__)
#define LOG_ERR(f, ...) printf("[ERR][" TAG "] " f "\n", ##__VA_ARGS__)
#define RETURN_IF_ERROR(x) do { int r_=(x); if(r_!=0) return r_; } while(0)

typedef struct vl53l0x_ctx {
    bool in_use;
    bool initialized;
    ch32_i2c_multi_node_t *node;
    uint8_t addr;
    uint32_t bridge_timeout_ms;
    uint32_t ready_timeout_ms;
    uint8_t stop_variable;
} vl53l0x_ctx_t;

static vl53l0x_ctx_t s_instances[VL53L0X_MAX_BRIDGE_INSTANCES];

static void delay_ms(uint32_t ms)
{
    TickType_t ticks = pdMS_TO_TICKS(ms);
    vTaskDelay(ticks == 0U ? 1U : ticks);
}

static int validate_node(const ch32_i2c_multi_node_t *node)
{
    if (node == NULL || !node->ready || node->token == 0U ||
        node->device_type != CH32_I2C_MULTI_DYN_DEVICE_TYPE_I2C ||
        node->node_id < 0x01U || node->node_id > 0x20U) {
        return ERR_INVALID_PARAM;
    }
    return 0;
}

static int map_gateway(ch32_i2c_multi_result_t result)
{
    switch (result) {
    case CH32_I2C_MULTI_RESULT_OK: return 0;
    case CH32_I2C_MULTI_RESULT_TIMEOUT:
    case CH32_I2C_MULTI_RESULT_NO_DATA: return ERR_TIMEOUT;
    case CH32_I2C_MULTI_RESULT_BUSY: return ERR_BUSY;
    case CH32_I2C_MULTI_RESULT_INVALID_ARG: return ERR_INVALID_PARAM;
    case CH32_I2C_MULTI_RESULT_NODE_NOT_FOUND: return ERR_NO_DEVICE;
    default: return ERR_VL53L0X_COMM;
    }
}

static int write_reg(vl53l0x_ctx_t *ctx, uint8_t reg,
                     const uint8_t *data, uint8_t len)
{
    ch32_i2c_multi_result_t result = CH32_I2C_MULTI_RESULT_COMM_FAIL;
    for (uint32_t attempt = 0U; attempt < VL53L0X_MAX_RETRIES; ++attempt) {
        if (validate_node(ctx->node) != 0) return ERR_NO_DEVICE;
        result = ch32_i2c_multi_write_reg_to(ctx->node, ctx->addr, reg,
                                             data, len);
        if (result == CH32_I2C_MULTI_RESULT_OK) return 0;
        if (attempt + 1U < VL53L0X_MAX_RETRIES) {
            LOG_WRN("downstream write retry=%lu token=0x%04X node=%u reg=0x%02X result=%s",
                    (unsigned long)(attempt + 1U), ctx->node->token,
                    ctx->node->node_id, reg,
                    ch32_i2c_multi_result_text(result));
            delay_ms(2U);
        }
    }
    return map_gateway(result);
}

static int read_reg(vl53l0x_ctx_t *ctx, uint8_t reg,
                    uint8_t *data, uint8_t len)
{
    ch32_i2c_multi_result_t result = CH32_I2C_MULTI_RESULT_COMM_FAIL;
    for (uint32_t attempt = 0U; attempt < VL53L0X_MAX_RETRIES; ++attempt) {
        if (validate_node(ctx->node) != 0) return ERR_NO_DEVICE;
        result = ch32_i2c_multi_read_regs_from(ctx->node, ctx->addr, reg,
                                               data, len);
        if (result == CH32_I2C_MULTI_RESULT_OK) return 0;
        if (attempt + 1U < VL53L0X_MAX_RETRIES) delay_ms(2U);
    }
    LOG_ERR("downstream read FAIL token=0x%04X node=%u reg=0x%02X result=%s",
            ctx->node->token, ctx->node->node_id, reg,
            ch32_i2c_multi_result_text(result));
    return map_gateway(result);
}

static int write_multi(vl53l0x_ctx_t *ctx, const uint8_t *data, uint8_t len)
{
    ch32_i2c_multi_result_t result = CH32_I2C_MULTI_RESULT_COMM_FAIL;
    for (uint32_t attempt = 0U; attempt < VL53L0X_MAX_RETRIES; ++attempt) {
        if (validate_node(ctx->node) != 0) return ERR_NO_DEVICE;
        result = ch32_i2c_multi_write_multi_to(ctx->node, ctx->addr,
                                               data, len);
        if (result == CH32_I2C_MULTI_RESULT_OK) return 0;
        if (attempt + 1U < VL53L0X_MAX_RETRIES) delay_ms(2U);
    }
    LOG_ERR("downstream multi-write FAIL token=0x%04X node=%u result=%s",
            ctx->node->token, ctx->node->node_id,
            ch32_i2c_multi_result_text(result));
    return map_gateway(result);
}

static int write_u8(vl53l0x_ctx_t *ctx, uint8_t reg, uint8_t value)
{ return write_reg(ctx, reg, &value, 1U); }

static int write_u16(vl53l0x_ctx_t *ctx, uint8_t reg, uint16_t value)
{
    uint8_t data[2] = {(uint8_t)(value >> 8), (uint8_t)value};
    return write_reg(ctx, reg, data, sizeof(data));
}

static int read_u8(vl53l0x_ctx_t *ctx, uint8_t reg, uint8_t *value)
{ return read_reg(ctx, reg, value, 1U); }

static int update_u8(vl53l0x_ctx_t *ctx, uint8_t reg,
                     uint8_t set_mask, uint8_t clear_mask)
{
    uint8_t value;
    RETURN_IF_ERROR(read_u8(ctx, reg, &value));
    return write_u8(ctx, reg, (uint8_t)((value | set_mask) & ~clear_mask));
}

static int wait_mask(vl53l0x_ctx_t *ctx, uint8_t reg, uint8_t mask,
                     uint32_t timeout_ms)
{
    uint8_t value;
    for (uint32_t elapsed = 0U; elapsed < timeout_ms;
         elapsed += VL53L0X_POLL_MS) {
        RETURN_IF_ERROR(read_u8(ctx, reg, &value));
        if ((value & mask) != 0U) return 0;
        delay_ms(VL53L0X_POLL_MS);
    }
    return ERR_VL53L0X_DATA_NOT_READY;
}

static int get_spad_info(vl53l0x_ctx_t *ctx, uint8_t *count, bool *aperture)
{
    uint8_t value;
    RETURN_IF_ERROR(write_u8(ctx,0x80,1)); RETURN_IF_ERROR(write_u8(ctx,0xFF,1));
    RETURN_IF_ERROR(write_u8(ctx,0,0)); RETURN_IF_ERROR(write_u8(ctx,0xFF,6));
    RETURN_IF_ERROR(update_u8(ctx,0x83,4,0)); RETURN_IF_ERROR(write_u8(ctx,0xFF,7));
    RETURN_IF_ERROR(write_u8(ctx,0x81,1)); RETURN_IF_ERROR(write_u8(ctx,0x80,1));
    RETURN_IF_ERROR(write_u8(ctx,0x94,0x6B)); RETURN_IF_ERROR(write_u8(ctx,0x83,0));
    RETURN_IF_ERROR(wait_mask(ctx,0x83,0xFF,1000)); RETURN_IF_ERROR(write_u8(ctx,0x83,1));
    RETURN_IF_ERROR(read_u8(ctx,0x92,&value)); *count=value&0x7F; *aperture=(value&0x80)!=0;
    RETURN_IF_ERROR(write_u8(ctx,0x81,0)); RETURN_IF_ERROR(write_u8(ctx,0xFF,6));
    RETURN_IF_ERROR(update_u8(ctx,0x83,0,4)); RETURN_IF_ERROR(write_u8(ctx,0xFF,1));
    RETURN_IF_ERROR(write_u8(ctx,0,1)); RETURN_IF_ERROR(write_u8(ctx,0xFF,0));
    return write_u8(ctx,0x80,0);
}

static int load_tuning(vl53l0x_ctx_t *ctx)
{
    static const uint8_t settings[][2] = {
        {0xFF,1},{0,0},{0xFF,0},{9,0},{0x10,0},{0x11,0},{0x24,1},{0x25,0xFF},{0x75,0},
        {0xFF,1},{0x4E,0x2C},{0x48,0},{0x30,0x20},{0xFF,0},{0x30,9},{0x54,0},{0x31,4},
        {0x32,3},{0x40,0x83},{0x46,0x25},{0x60,0},{0x27,0},{0x50,6},{0x51,0},{0x52,0x96},
        {0x56,8},{0x57,0x30},{0x61,0},{0x62,0},{0x64,0},{0x65,0},{0x66,0xA0},{0xFF,1},
        {0x22,0x32},{0x47,0x14},{0x49,0xFF},{0x4A,0},{0xFF,0},{0x7A,0x0A},{0x7B,0},
        {0x78,0x21},{0xFF,1},{0x23,0x34},{0x42,0},{0x44,0xFF},{0x45,0x26},{0x46,5},
        {0x40,0x40},{0x0E,6},{0x20,0x1A},{0x43,0x40},{0xFF,0},{0x34,3},{0x35,0x44},
        {0xFF,1},{0x31,4},{0x4B,9},{0x4C,5},{0x4D,4},{0xFF,0},{0x44,0},{0x45,0x20},
        {0x47,8},{0x48,0x28},{0x67,0},{0x70,4},{0x71,1},{0x72,0xFE},{0x76,0},{0x77,0},
        {0xFF,1},{0x0D,1},{0xFF,0},{0x80,1},{1,0xF8},{0xFF,1},{0x8E,1},{0,1},{0xFF,0},{0x80,0}
    };
    for (size_t index=0; index<sizeof(settings)/sizeof(settings[0]); ++index)
        RETURN_IF_ERROR(write_u8(ctx, settings[index][0], settings[index][1]));
    return 0;
}

static int calibrate(vl53l0x_ctx_t *ctx, uint8_t vhv)
{
    RETURN_IF_ERROR(write_u8(ctx,REG_SYSRANGE_START,(uint8_t)(1U|vhv)));
    RETURN_IF_ERROR(wait_mask(ctx,REG_INTERRUPT_STATUS,7U,1000U));
    RETURN_IF_ERROR(write_u8(ctx,REG_INTERRUPT_CLEAR,1U));
    return write_u8(ctx,REG_SYSRANGE_START,0U);
}

static int sensor_init(vl53l0x_ctx_t *ctx)
{
    uint8_t id,value,count,enabled=0U,map[6]; bool aperture;
    RETURN_IF_ERROR(read_u8(ctx,REG_MODEL_ID,&id));
    if(id!=VL53L0X_MODEL_ID){LOG_ERR("module init FAIL stage=id expected=0x%02X got=0x%02X",VL53L0X_MODEL_ID,id);return ERR_VL53L0X_ID_MISMATCH;}
    RETURN_IF_ERROR(update_u8(ctx,REG_EXTSUP_HV,1,0)); RETURN_IF_ERROR(write_u8(ctx,0x88,0));
    RETURN_IF_ERROR(write_u8(ctx,0x80,1)); RETURN_IF_ERROR(write_u8(ctx,0xFF,1)); RETURN_IF_ERROR(write_u8(ctx,0,0));
    RETURN_IF_ERROR(read_u8(ctx,0x91,&ctx->stop_variable)); RETURN_IF_ERROR(write_u8(ctx,0,1));
    RETURN_IF_ERROR(write_u8(ctx,0xFF,0)); RETURN_IF_ERROR(write_u8(ctx,0x80,0));
    RETURN_IF_ERROR(update_u8(ctx,REG_MSRC_CONTROL,0x12,0)); RETURN_IF_ERROR(write_u16(ctx,REG_RATE_LIMIT,32));
    RETURN_IF_ERROR(write_u8(ctx,REG_SEQUENCE_CONFIG,0xFF)); RETURN_IF_ERROR(get_spad_info(ctx,&count,&aperture));
    RETURN_IF_ERROR(read_reg(ctx,REG_SPAD_MAP,map,6)); RETURN_IF_ERROR(write_u8(ctx,0xFF,1));
    RETURN_IF_ERROR(write_u8(ctx,0x4F,0)); RETURN_IF_ERROR(write_u8(ctx,0x4E,0x2C));
    RETURN_IF_ERROR(write_u8(ctx,0xFF,0)); RETURN_IF_ERROR(write_u8(ctx,0xB6,0xB4));
    for(uint8_t i=0;i<48;i++){if(i<(aperture?12U:0U)||enabled==count)map[i/8]&=(uint8_t)~(1U<<(i%8));else if((map[i/8]>>(i%8))&1U)enabled++;}
    { uint8_t transfer[7] = {REG_SPAD_MAP}; memcpy(&transfer[1],map,6);
      RETURN_IF_ERROR(write_multi(ctx,transfer,sizeof(transfer))); }
    RETURN_IF_ERROR(load_tuning(ctx));
    RETURN_IF_ERROR(write_u8(ctx,REG_INTERRUPT_CONFIG,4)); RETURN_IF_ERROR(read_u8(ctx,REG_GPIO_MUX,&value));
    RETURN_IF_ERROR(write_u8(ctx,REG_GPIO_MUX,(uint8_t)(value&~0x10U))); RETURN_IF_ERROR(write_u8(ctx,REG_INTERRUPT_CLEAR,1));
    RETURN_IF_ERROR(write_u8(ctx,REG_SEQUENCE_CONFIG,0xE8)); RETURN_IF_ERROR(write_u8(ctx,REG_SEQUENCE_CONFIG,1));
    RETURN_IF_ERROR(calibrate(ctx,0x40)); RETURN_IF_ERROR(write_u8(ctx,REG_SEQUENCE_CONFIG,2));
    RETURN_IF_ERROR(calibrate(ctx,0)); RETURN_IF_ERROR(write_u8(ctx,REG_SEQUENCE_CONFIG,0xE8));
    RETURN_IF_ERROR(write_u8(ctx,0x80,1)); RETURN_IF_ERROR(write_u8(ctx,0xFF,1)); RETURN_IF_ERROR(write_u8(ctx,0,0));
    RETURN_IF_ERROR(write_u8(ctx,0x91,ctx->stop_variable)); RETURN_IF_ERROR(write_u8(ctx,0,1));
    RETURN_IF_ERROR(write_u8(ctx,0xFF,0)); RETURN_IF_ERROR(write_u8(ctx,0x80,0));
    return write_u8(ctx,REG_SYSRANGE_START,2);
}

int vl53l0x_init(vl53l0x_handle_t *handle,const vl53l0x_cfg_t *cfg)
{
    vl53l0x_ctx_t *ctx=NULL; bool found=false; ch32_i2c_multi_result_t gateway;
    if(handle==NULL||cfg==NULL||cfg->i2c_addr!=VL53L0X_I2C_ADDR_DEFAULT||
       cfg->bridge_timeout_ms==0U||cfg->data_ready_timeout_ms==0U||validate_node(cfg->ch32_node)!=0)return ERR_INVALID_PARAM;
    *handle=NULL;
    for(size_t i=0;i<VL53L0X_MAX_BRIDGE_INSTANCES;i++)if(!s_instances[i].in_use){ctx=&s_instances[i];break;}
    if(ctx==NULL)return ERR_BUSY;
    memset(ctx,0,sizeof(*ctx));ctx->in_use=true;ctx->node=cfg->ch32_node;ctx->addr=cfg->i2c_addr;
    ctx->bridge_timeout_ms=cfg->bridge_timeout_ms;ctx->ready_timeout_ms=cfg->data_ready_timeout_ms;
    gateway=ch32_i2c_multi_set_speed_400k(ctx->node);
    if(gateway==CH32_I2C_MULTI_RESULT_OK)gateway=ch32_i2c_multi_probe(ctx->node,ctx->addr,&found);
    if(gateway!=CH32_I2C_MULTI_RESULT_OK||!found){int r=gateway==CH32_I2C_MULTI_RESULT_OK?ERR_NO_DEVICE:map_gateway(gateway);LOG_ERR("module init FAIL stage=downstream_probe token=0x%04X node=%u result=%s",ctx->node->token,ctx->node->node_id,ch32_i2c_multi_result_text(gateway));memset(ctx,0,sizeof(*ctx));return r;}
    int result=sensor_init(ctx);if(result!=0){LOG_ERR("module init FAIL stage=sensor_setup token=0x%04X node=%u err=%d",ctx->node->token,ctx->node->node_id,result);memset(ctx,0,sizeof(*ctx));return result;}
    ctx->initialized=true;*handle=ctx;LOG_INF("init OK addr=0x29 id=0xEE token=0x%04X node=%u",ctx->node->token,ctx->node->node_id);return 0;
}

int vl53l0x_deinit(vl53l0x_handle_t handle)
{if(handle==NULL||!handle->in_use||!handle->initialized)return ERR_NOT_INIT;int r=write_u8(handle,REG_SYSRANGE_START,1);LOG_INF("deinit %s token=0x%04X node=%u",r==0?"OK":"FAIL",handle->node->token,handle->node->node_id);memset(handle,0,sizeof(*handle));return r;}

int vl53l0x_read(vl53l0x_handle_t handle,vl53l0x_result_t *out)
{uint8_t data[12];if(handle==NULL||out==NULL)return ERR_INVALID_PARAM;if(!handle->initialized)return ERR_NOT_INIT;memset(out,0,sizeof(*out));RETURN_IF_ERROR(wait_mask(handle,REG_INTERRUPT_STATUS,7,handle->ready_timeout_ms));RETURN_IF_ERROR(read_reg(handle,REG_RANGE_STATUS,data,12));RETURN_IF_ERROR(write_u8(handle,REG_INTERRUPT_CLEAR,1));out->range_status=(data[0]&0x78U)>>3;out->distance_mm=((uint16_t)data[10]<<8)|data[11];out->valid=out->distance_mm<=VL53L0X_RANGE_MAX_MM;out->out_of_range=!out->valid;return out->valid?0:ERR_VL53L0X_OUT_OF_RANGE;}
