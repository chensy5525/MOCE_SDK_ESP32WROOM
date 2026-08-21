#include "portal_dns_server.h"

#include <errno.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "lwip/sockets.h"

#define PORTAL_DNS_PORT             53U
#define PORTAL_DNS_PACKET_MAX_LEN   512U
#define PORTAL_DNS_HEADER_LEN       12U
#define PORTAL_DNS_QUESTION_LEN     4U
#define PORTAL_DNS_ANSWER_LEN       16U
#define PORTAL_DNS_TASK_STACK_SIZE  3072U
#define PORTAL_DNS_TASK_PRIORITY    4U
#define PORTAL_DNS_STOP_TIMEOUT_MS  500U
#define PORTAL_DNS_TTL_SECONDS      60U
#define PORTAL_DNS_TYPE_A           1U
#define PORTAL_DNS_CLASS_IN         1U

struct PortalDnsServer {
    volatile bool running;
    int socket_fd;
    uint32_t ipv4_address;
    TaskHandle_t task;
    SemaphoreHandle_t stopped;
};

static const char *TAG = "portal_dns";

static uint16_t read_network_u16(const uint8_t *source)
{
    uint16_t value;
    memcpy(&value, source, sizeof(value));
    return ntohs(value);
}

static void write_network_u16(uint8_t *destination, uint16_t value)
{
    value = htons(value);
    memcpy(destination, &value, sizeof(value));
}

static void write_network_u32(uint8_t *destination, uint32_t value)
{
    value = htonl(value);
    memcpy(destination, &value, sizeof(value));
}

static bool find_question_end(const uint8_t *packet,
                              size_t packet_length,
                              size_t *question_end)
{
    size_t cursor = PORTAL_DNS_HEADER_LEN;

    while (cursor < packet_length) {
        uint8_t label_length = packet[cursor++];
        if (label_length == 0U) {
            if ((packet_length - cursor) < PORTAL_DNS_QUESTION_LEN) {
                return false;
            }
            *question_end = cursor + PORTAL_DNS_QUESTION_LEN;
            return true;
        }
        if ((label_length > 63U) || (label_length > (packet_length - cursor))) {
            return false;
        }
        cursor += label_length;
    }
    return false;
}

static size_t build_dns_reply(const uint8_t *request,
                              size_t request_length,
                              uint8_t *reply,
                              size_t reply_capacity,
                              uint32_t ipv4_address)
{
    size_t question_end;
    uint16_t request_flags;
    uint16_t query_type;
    uint16_t query_class;

    if ((request_length < PORTAL_DNS_HEADER_LEN) ||
        (request_length > reply_capacity) ||
        (read_network_u16(&request[4]) != 1U) ||
        !find_question_end(request, request_length, &question_end)) {
        return 0U;
    }

    request_flags = read_network_u16(&request[2]);
    if ((request_flags & UINT16_C(0x7800)) != 0U) {
        return 0U;
    }
    query_type = read_network_u16(&request[question_end - 4U]);
    query_class = read_network_u16(&request[question_end - 2U]);

    /* Copy only the header and question. EDNS/additional records from the
     * request must not be placed before the answer section. */
    memcpy(reply, request, question_end);
    write_network_u16(&reply[2],
                      (uint16_t)((request_flags & UINT16_C(0x0100)) |
                                 UINT16_C(0x8400)));
    write_network_u16(&reply[6], 0U);
    write_network_u16(&reply[8], 0U);
    write_network_u16(&reply[10], 0U);

    if ((query_type != PORTAL_DNS_TYPE_A) ||
        (query_class != PORTAL_DNS_CLASS_IN)) {
        return question_end;
    }
    if ((question_end + PORTAL_DNS_ANSWER_LEN) > reply_capacity) {
        return 0U;
    }

    write_network_u16(&reply[6], 1U);
    write_network_u16(&reply[question_end], UINT16_C(0xC00C));
    write_network_u16(&reply[question_end + 2U], PORTAL_DNS_TYPE_A);
    write_network_u16(&reply[question_end + 4U], PORTAL_DNS_CLASS_IN);
    write_network_u32(&reply[question_end + 6U], PORTAL_DNS_TTL_SECONDS);
    write_network_u16(&reply[question_end + 10U], sizeof(ipv4_address));
    memcpy(&reply[question_end + 12U],
           &ipv4_address,
           sizeof(ipv4_address));
    return question_end + PORTAL_DNS_ANSWER_LEN;
}

static void portal_dns_task(void *argument)
{
    PortalDnsServer *server = argument;
    uint8_t request[PORTAL_DNS_PACKET_MAX_LEN];
    uint8_t reply[PORTAL_DNS_PACKET_MAX_LEN];

    while (server->running) {
        struct sockaddr_storage source_address = {0};
        socklen_t source_length = sizeof(source_address);
        int received = recvfrom(server->socket_fd,
                                request,
                                sizeof(request),
                                0,
                                (struct sockaddr *)&source_address,
                                &source_length);
        if (received < 0) {
            if (server->running &&
                (errno != EAGAIN) && (errno != EWOULDBLOCK)) {
                ESP_LOGW(TAG, "DNS receive failed: errno=%d", errno);
            }
            continue;
        }

        size_t reply_length = build_dns_reply(request,
                                              (size_t)received,
                                              reply,
                                              sizeof(reply),
                                              server->ipv4_address);
        if (reply_length > 0U) {
            int sent = sendto(server->socket_fd,
                              reply,
                              reply_length,
                              0,
                              (struct sockaddr *)&source_address,
                              source_length);
            if ((sent < 0) && server->running) {
                ESP_LOGW(TAG, "DNS response failed: errno=%d", errno);
            }
        }
    }

    xSemaphoreGive(server->stopped);
    vTaskDelete(NULL);
}

esp_err_t portal_dns_server_start(uint32_t ipv4_address,
                                  PortalDnsServer **server_out)
{
    const struct timeval receive_timeout = {
        .tv_sec = 0,
        .tv_usec = 200000,
    };
    const struct sockaddr_in bind_address = {
        .sin_family = AF_INET,
        .sin_port = htons(PORTAL_DNS_PORT),
        .sin_addr.s_addr = htonl(INADDR_ANY),
    };
    PortalDnsServer *server;

    if ((ipv4_address == 0U) || (server_out == NULL)) {
        return ESP_ERR_INVALID_ARG;
    }
    *server_out = NULL;
    server = calloc(1U, sizeof(*server));
    if (server == NULL) {
        return ESP_ERR_NO_MEM;
    }
    server->socket_fd = -1;
    server->ipv4_address = ipv4_address;
    server->stopped = xSemaphoreCreateBinary();
    if (server->stopped == NULL) {
        free(server);
        return ESP_ERR_NO_MEM;
    }

    server->socket_fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
    if (server->socket_fd < 0) {
        portal_dns_server_stop(server);
        return ESP_FAIL;
    }
    if ((setsockopt(server->socket_fd,
                    SOL_SOCKET,
                    SO_RCVTIMEO,
                    &receive_timeout,
                    sizeof(receive_timeout)) < 0) ||
        (bind(server->socket_fd,
              (const struct sockaddr *)&bind_address,
              sizeof(bind_address)) < 0)) {
        ESP_LOGE(TAG, "DNS socket setup failed: errno=%d", errno);
        portal_dns_server_stop(server);
        return ESP_FAIL;
    }

    server->running = true;
    if (xTaskCreate(portal_dns_task,
                    "portal_dns",
                    PORTAL_DNS_TASK_STACK_SIZE,
                    server,
                    PORTAL_DNS_TASK_PRIORITY,
                    &server->task) != pdPASS) {
        server->running = false;
        portal_dns_server_stop(server);
        return ESP_ERR_NO_MEM;
    }

    *server_out = server;
    ESP_LOGI(TAG, "wildcard DNS redirect ready on UDP/53");
    return ESP_OK;
}

void portal_dns_server_stop(PortalDnsServer *server)
{
    if (server == NULL) {
        return;
    }
    server->running = false;
    if (server->socket_fd >= 0) {
        shutdown(server->socket_fd, SHUT_RDWR);
        close(server->socket_fd);
        server->socket_fd = -1;
    }
    if (server->task != NULL) {
        if (xSemaphoreTake(server->stopped,
                           pdMS_TO_TICKS(PORTAL_DNS_STOP_TIMEOUT_MS)) != pdTRUE) {
            vTaskDelete(server->task);
        }
    }
    vSemaphoreDelete(server->stopped);
    free(server);
}
