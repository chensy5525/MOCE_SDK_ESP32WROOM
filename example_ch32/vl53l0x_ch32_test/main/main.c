#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "ch32_i2c_multi_gateway_final.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "ch32_vl53l0x_gateway.h"

#define TAG "VL53L0X_EXAMPLE"
#define MAX_NODES 6U
#define MAX_SENSORS 6U
#define DISCOVERY_START_MS 5000U
#define DISCOVERY_RETRY_MS 3000U
#define REDISCOVERY_MS 5000U
#define SAMPLE_PERIOD_MS 200U
#define SAMPLE_COUNT 50U

typedef struct { ch32_i2c_multi_node_t *node; vl53l0x_handle_t sensor; uint32_t samples; } sensor_slot_t;
static ch32_i2c_multi_node_t s_nodes[MAX_NODES];
static size_t s_node_count;
static sensor_slot_t s_slots[MAX_SENSORS];

static ch32_i2c_multi_node_t *find_node(uint8_t type,uint16_t token)
{for(size_t i=0;i<s_node_count;i++)if(s_nodes[i].device_type==type&&s_nodes[i].token==token)return &s_nodes[i];return NULL;}

static ch32_i2c_multi_node_t *merge_node(const ch32_i2c_multi_node_t *fresh)
{
    if(fresh==NULL||!fresh->ready||fresh->token==0U)return NULL;
    ch32_i2c_multi_node_t *stable=find_node(fresh->device_type,fresh->token);
    if(stable!=NULL){uint8_t old=stable->node_id;*stable=*fresh;if(old!=stable->node_id)printf("[INF][CH32_I2C] node update token=0x%04X old=%u new=%u stable_ref_kept=1\n",stable->token,old,stable->node_id);return stable;}
    if(s_node_count>=MAX_NODES){printf("[ERR][" TAG "] stable node table full token=0x%04X\n",fresh->token);return NULL;}
    stable=&s_nodes[s_node_count++];*stable=*fresh;
    printf("[INF][CH32_I2C] F2 confirmed node add token=0x%04X node=%u total=%u\n",stable->token,stable->node_id,(unsigned)s_node_count);
    return stable;
}

static sensor_slot_t *slot_for(ch32_i2c_multi_node_t *node)
{sensor_slot_t *free_slot=NULL;for(size_t i=0;i<MAX_SENSORS;i++){if(s_slots[i].node==node)return &s_slots[i];if(free_slot==NULL&&s_slots[i].node==NULL)free_slot=&s_slots[i];}if(free_slot)free_slot->node=node;return free_slot;}

static void start_sensor(ch32_i2c_multi_node_t *node)
{
    sensor_slot_t *slot=slot_for(node);if(slot==NULL||slot->sensor!=NULL||slot->samples>=SAMPLE_COUNT)return;
    vl53l0x_cfg_t cfg={.i2c_addr=VL53L0X_I2C_ADDR_DEFAULT,.ch32_node=node,.bridge_timeout_ms=VL53L0X_BRIDGE_TIMEOUT_MS,.data_ready_timeout_ms=VL53L0X_DATA_READY_TIMEOUT_MS};
    int result=vl53l0x_init(&slot->sensor,&cfg);
    if(result!=0){printf("[ERR][" TAG "] module init FAIL token=0x%04X node=%u err=%d\n",node->token,node->node_id,result);slot->node=NULL;}
}

static void discover_merge(uint32_t timeout_ms)
{
    ch32_i2c_multi_node_t fresh[MAX_NODES]={0};size_t count=0;
    ch32_i2c_multi_result_t result=ch32_i2c_multi_discover_incremental(fresh,MAX_NODES,&count,timeout_ms);
    printf("[INF][CH32_I2C] discovery result=%s fresh=%u stable_before=%u\n",ch32_i2c_multi_result_text(result),(unsigned)count,(unsigned)s_node_count);
    if(result!=CH32_I2C_MULTI_RESULT_OK&&count==0U){printf("[WRN][CH32_I2C] discovery keep existing stable=%u reason=FAILED_OR_TIMEOUT\n",(unsigned)s_node_count);return;}
    for(size_t i=0;i<count;i++){
        ch32_i2c_multi_node_t *node=merge_node(&fresh[i]);if(node==NULL)continue;
        bool found=false;result=ch32_i2c_multi_probe(node,VL53L0X_I2C_ADDR_DEFAULT,&found);
        if(result!=CH32_I2C_MULTI_RESULT_OK){printf("[ERR][CH32_I2C] downstream probe FAIL token=0x%04X node=%u addr=0x29 result=%s\n",node->token,node->node_id,ch32_i2c_multi_result_text(result));continue;}
        if(!found){printf("[INF][CH32_I2C] downstream scan node=%u token=0x%04X result=OK addr_0x29=ABSENT\n",node->node_id,node->token);continue;}
        printf("[INF][CH32_I2C] downstream scan node=%u token=0x%04X result=OK addr_0x29=PRESENT\n",node->node_id,node->token);
        start_sensor(node);
    }
}

void app_main(void)
{
    ch32_i2c_multi_config_t cfg;ch32_i2c_multi_default_config(&cfg);cfg.discovery_timeout_ms=DISCOVERY_START_MS;cfg.command_timeout_ms=VL53L0X_BRIDGE_TIMEOUT_MS;
    if(ch32_i2c_multi_init(&cfg)!=0){printf("[ERR][CH32_CAN_CORE] init FAIL\n");return;}
    printf("[INF][CH32_CAN_CORE] init OK\n");discover_merge(DISCOVERY_START_MS);
    TickType_t last_rediscovery=xTaskGetTickCount();
    while(true){
        for(size_t i=0;i<MAX_SENSORS;i++)if(s_slots[i].sensor!=NULL&&s_slots[i].samples<SAMPLE_COUNT){vl53l0x_result_t data;int r=vl53l0x_read(s_slots[i].sensor,&data);if(r==0)printf("[INF][VL53L0X] data valid=1 mm=%u out_of_range=0 status=%u token=0x%04X node=%u\n",data.distance_mm,data.range_status,s_slots[i].node->token,s_slots[i].node->node_id);else if(r==ERR_VL53L0X_OUT_OF_RANGE)printf("[WRN][VL53L0X] OUT RANGE mm=%u status=%u token=0x%04X node=%u\n",data.distance_mm,data.range_status,s_slots[i].node->token,s_slots[i].node->node_id);else printf("[ERR][VL53L0X] module runtime FAIL token=0x%04X node=%u err=%d\n",s_slots[i].node->token,s_slots[i].node->node_id,r);s_slots[i].samples++;if(s_slots[i].samples==SAMPLE_COUNT){(void)vl53l0x_deinit(s_slots[i].sensor);s_slots[i].sensor=NULL;}}
        if((xTaskGetTickCount()-last_rediscovery)>=pdMS_TO_TICKS(REDISCOVERY_MS)){printf("[INF][" TAG "] rediscovery begin keep_existing_i2c=1\n");discover_merge(DISCOVERY_RETRY_MS);last_rediscovery=xTaskGetTickCount();}
        vTaskDelay(pdMS_TO_TICKS(SAMPLE_PERIOD_MS));
    }
}
