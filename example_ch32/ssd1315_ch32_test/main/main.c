#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "ch32_i2c_multi_gateway_final.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "ssd1315.h"

#define SSD1315_EXAMPLE_MAX_NODES            6U
#define SSD1315_EXAMPLE_MAX_DISPLAYS         6U
#define SSD1315_EXAMPLE_DISCOVERY_START_MS   5000U
#define SSD1315_EXAMPLE_DISCOVERY_RETRY_MS   3000U
#define SSD1315_EXAMPLE_REDISCOVERY_MS       5000U
#define SSD1315_EXAMPLE_TEXT_X                0U
#define SSD1315_EXAMPLE_TEXT_PAGE             0U

typedef struct {
	ch32_i2c_multi_node_t *node;
	ssd1315_handle_t display;
} ssd1315_example_slot_t;

static ch32_i2c_multi_node_t s_stable_nodes[SSD1315_EXAMPLE_MAX_NODES];
static size_t s_stable_node_count;
static ssd1315_example_slot_t s_displays[SSD1315_EXAMPLE_MAX_DISPLAYS];

static ch32_i2c_multi_node_t *ssd1315_find_node_by_identity(

    uint8_t device_type, uint16_t token) {
	size_t index;

	if (token == 0U) {
		return NULL;
	}
	for (index = 0U; index < s_stable_node_count; ++index) {
		if (s_stable_nodes[index].device_type == device_type &&
		        s_stable_nodes[index].token == token) {
			return &s_stable_nodes[index];
		}
	}
	return NULL;
}

static ch32_i2c_multi_node_t *ssd1315_merge_stable_node(

    const ch32_i2c_multi_node_t *fresh) {
	ch32_i2c_multi_node_t *stable;
	uint8_t old_node_id;

	if (fresh == NULL || !fresh->ready || fresh->token == 0U) {
		return NULL;
	}

	stable = ssd1315_find_node_by_identity(fresh->device_type, fresh->token);
	if (stable != NULL) {
		old_node_id = stable->node_id;
		*stable = *fresh;
		if (old_node_id != stable->node_id) {
			printf("[INF][SSD1315_EXAMPLE] node update token=0x%04X old=%u new=%u\n",
			       stable->token, old_node_id, stable->node_id);
		}
		return stable;
	}

	if (s_stable_node_count >= SSD1315_EXAMPLE_MAX_NODES) {
		printf("[ERR][SSD1315_EXAMPLE] node table full token=0x%04X\n",
		       fresh->token);
		return NULL;
	}

	stable = &s_stable_nodes[s_stable_node_count];
	*stable = *fresh;
	++s_stable_node_count;
	printf("[INF][SSD1315_EXAMPLE] node add token=0x%04X node=%u\n",
	       stable->token, stable->node_id);
	return stable;
}

static bool ssd1315_node_has_address(const ch32_i2c_multi_node_t *node,

                                     uint8_t address) {
	uint8_t index;

	for (index = 0U; node != NULL && index < node->i2c_addr_count; ++index) {
		if (node->i2c_addrs[index] == address) {
			return true;
		}
	}
	return false;
}

static ssd1315_example_slot_t *ssd1315_find_display_slot(

    const ch32_i2c_multi_node_t *node) {
	size_t index;
	ssd1315_example_slot_t *free_slot = NULL;

	for (index = 0U; index < SSD1315_EXAMPLE_MAX_DISPLAYS; ++index) {
		if (s_displays[index].node == node) {
			return &s_displays[index];
		}
		if (free_slot == NULL && s_displays[index].node == NULL) {
			free_slot = &s_displays[index];
		}
	}
	if (free_slot != NULL) {
		free_slot->node = (ch32_i2c_multi_node_t *)node;
	}
	return free_slot;
}

static void ssd1315_start_display(ch32_i2c_multi_node_t *node) {
	ssd1315_example_slot_t *slot;
	ssd1315_cfg_t config;
	int result;

	slot = ssd1315_find_display_slot(node);
	if (slot == NULL || slot->display != NULL) {
		return;
	}

	memset(&config, 0, sizeof(config));
	config.i2c_addr = SSD1315_I2C_ADDR;
	config.clk_speed_hz = SSD1315_I2C_FREQ_HZ;
	config.ch32_node = node;
	config.bridge_timeout_ms = SSD1315_BRIDGE_TIMEOUT_MS;

	result = ssd1315_init_device(&slot->display, &config);
	if (result == 0) {
		result = ssd1315_draw_text(slot->display, SSD1315_EXAMPLE_TEXT_X,
		                           SSD1315_EXAMPLE_TEXT_PAGE, "SSD1315 OK1");
	}
	if (result == 0) {
		result = ssd1315_refresh_display(slot->display);
	}
	if (result != 0) {
		printf("[ERR][SSD1315_EXAMPLE] display init failed token=0x%04X node=%u err=%d\n",
		       node->token, node->node_id, result);
		if (slot->display != NULL) {
			(void)ssd1315_deinit_device(slot->display);
			slot->display = NULL;
		}
	}
}

static void ssd1315_discover_and_merge(uint32_t timeout_ms) {
	ch32_i2c_multi_node_t fresh_nodes[SSD1315_EXAMPLE_MAX_NODES] = {0};
	ch32_i2c_multi_node_t *stable;
	ch32_i2c_multi_result_t gateway_result;
	size_t fresh_count = 0U;
	size_t index;
	bool oled_found;

	gateway_result = ch32_i2c_multi_discover_incremental(
	                     fresh_nodes, SSD1315_EXAMPLE_MAX_NODES, &fresh_count, timeout_ms);
	printf("[INF][SSD1315_EXAMPLE] discovery result=%s fresh=%u stable=%u\n",
	       ch32_i2c_multi_result_text(gateway_result),
	       (unsigned)fresh_count, (unsigned)s_stable_node_count);

	if (gateway_result != CH32_I2C_MULTI_RESULT_OK && fresh_count == 0U) {
		printf("[WRN][SSD1315_EXAMPLE] discovery failed, keeping stable table\n");
		return;
	}

	for (index = 0U; index < fresh_count; ++index) {
		stable = ssd1315_merge_stable_node(&fresh_nodes[index]);
		if (stable == NULL) {
			continue;
		}
		oled_found = ssd1315_node_has_address(stable, SSD1315_I2C_ADDR);
		if (!oled_found) {
			gateway_result = ch32_i2c_multi_probe(
			                     stable, SSD1315_I2C_ADDR, &oled_found);
			if (gateway_result != CH32_I2C_MULTI_RESULT_OK) {
				printf("[WRN][SSD1315_EXAMPLE] OLED probe communication failed token=0x%04X node=%u result=%s\n",
				       stable->token, stable->node_id,
				       ch32_i2c_multi_result_text(gateway_result));
				continue;
			}
			if (oled_found &&
			        stable->i2c_addr_count < CH32_I2C_MULTI_MAX_ADDRS_PER_NODE) {
				stable->i2c_addrs[stable->i2c_addr_count++] = SSD1315_I2C_ADDR;
			}
		}
		if (oled_found) {
			ssd1315_start_display(stable);
		} else {
			printf("[INF][SSD1315_EXAMPLE] CH32 gateway ready, no OLED at 0x%02X token=0x%04X node=%u\n",
			       SSD1315_I2C_ADDR, stable->token, stable->node_id);
		}
	}
}

void app_main(void) {
	ch32_i2c_multi_config_t gateway_config;

	ch32_i2c_multi_default_config(&gateway_config);
	gateway_config.discovery_timeout_ms = SSD1315_EXAMPLE_DISCOVERY_START_MS;
	gateway_config.command_timeout_ms = SSD1315_BRIDGE_TIMEOUT_MS;
	if (ch32_i2c_multi_init(&gateway_config) != 0) {
		printf("[ERR][SSD1315_EXAMPLE] shared CAN gateway init failed\n");
		return;
	}

	ssd1315_discover_and_merge(SSD1315_EXAMPLE_DISCOVERY_START_MS);
	while (true) {
		vTaskDelay(pdMS_TO_TICKS(SSD1315_EXAMPLE_REDISCOVERY_MS));
		ssd1315_discover_and_merge(SSD1315_EXAMPLE_DISCOVERY_RETRY_MS);
	}
}
