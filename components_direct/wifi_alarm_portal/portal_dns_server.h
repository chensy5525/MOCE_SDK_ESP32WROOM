#pragma once

#include <stdint.h>

#include "esp_err.h"

typedef struct PortalDnsServer PortalDnsServer;

esp_err_t portal_dns_server_start(uint32_t ipv4_address,
                                  PortalDnsServer **server);
void portal_dns_server_stop(PortalDnsServer *server);
