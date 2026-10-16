// #include <ctype.h>
// #include <errno.h>
// #include <stdatomic.h>
// #include <stdbool.h>
// #include <stdint.h>
// #include <stdio.h>
// #include <stdlib.h>
// #include <string.h>
// #include <unistd.h>

// #include "com.h"
// #include "cJSON.h"
// #include "esp_event.h"
// #include "esp_http_server.h"
// #include "esp_log.h"
// #include "esp_netif.h"
// #include "esp_timer.h"
// #include "esp_wifi.h"
// #include "mdns.h"
// #include "freertos/FreeRTOS.h"
// #include "freertos/event_groups.h"
// #include "freertos/queue.h"
// #include "freertos/task.h"
// #include "lwip/inet.h"
// #include "lwip/sockets.h"
// #include "nvs.h"
// #include "nvs_flash.h"
#include "com.h"

// #define SETUP_AP_SSID "ESP32_PORT"
// #define SETUP_AP_PASSWORD "123456789"
// #define APP_SERVER_PORT 8080
// #define PROVISIONING_MAX_BODY 512
// #define WIFI_CONNECTED_BIT BIT0
// #define TCP_RETRY_DELAY_MS 3000
// #define TCP_ACK_TIMEOUT_MS 3000
// #define TCP_MAX_PACKET_RETRIES 5
// #define MDNS_QUERY_TIMEOUT_MS 3000

// #define SERVER_PACKET_MAX_SIZE 256
// #define SERVER_PACKET_QUEUE_LENGTH 4
// #define APP_COMMAND_QUEUE_LENGTH 4
// #define DEVICE_ID "ESP32_0001"

// typedef struct {
//     size_t length;
//     uint8_t data[SERVER_PACKET_MAX_SIZE];
// } server_packet_t;


typedef struct {
    char ssid[33];
    char password[64];
    char app_id[65];
} device_config_t;

static const char *TAG = "solarconnect";
static EventGroupHandle_t s_wifi_events;
static esp_netif_t *s_sta_netif;
static httpd_handle_t s_http_server;
static device_config_t s_device_config;
static atomic_bool s_has_credentials = ATOMIC_VAR_INIT(false);
QueueHandle_t s_server_packet_queue;
QueueHandle_t s_app_command_queue;
QueueHandle_t s_app_response_queue;
static char s_device_id[32];
static uint32_t s_next_request_id;

static void init_device_id(void)
{
    nvs_handle_t handle;

    //Existing "solarconnect" NVS namespace 
    ESP_ERROR_CHECK(nvs_open("solarconnect",NVS_READWRITE, &handle));

    size_t length =sizeof(s_device_id);

    esp_err_t err =nvs_get_str(handle,"device_id",s_device_id, &length);

    if(err==ESP_ERR_NVS_NOT_FOUND){
        // first time app id set on this board

        snprintf(s_device_id,sizeof(s_device_id),"%s",
            DEVICE_ID);

        ESP_ERROR_CHECK(nvs_set_str(handle,"device_id",s_device_id));
        ESP_ERROR_CHECK(nvs_commit(handle));
    } else {
        //NVS error  report 
        ESP_ERROR_CHECK(err);
    }
    nvs_close(handle) ;

    //ESP_IDE terminal id print
    ESP_LOGI(TAG,"Device ID: %s",s_device_id);

}

// bool queue_server_packet(const uint8_t *data, size_t length)
// {
//     if (s_server_packet_queue == NULL || data == NULL ||
//         length == 0 || length > SERVER_PACKET_MAX_SIZE) {
//         return false;
//     }

//     server_packet_t packet = {.length = length};
//     memcpy(packet.data, data, length);
//     return xQueueSend(s_server_packet_queue, &packet, 0) == pdTRUE;
// }

static bool build_test_display_packet(const char *version,
                                      const char *table_number,
                                      server_packet_t *packet)
{
    static const char test_payload[] = "TEST";
    char frame[SERVER_PACKET_MAX_SIZE];
    int frame_length = snprintf(
        frame, sizeof(frame), "##%s030202%s04%s",
        version, table_number, test_payload
    );
    if (frame_length <= 0 ||
        (size_t)frame_length + 4 >= sizeof(frame)) {
        return false;
    }

    uint16_t crc = 0xFFFF;
    for (int i = 0; i < frame_length; i++) {
        crc ^= (uint8_t)frame[i];
        for (uint8_t bit = 0; bit < 8; bit++) {
            crc = (crc & 1U) ? (crc >> 1) ^ 0xA001U : crc >> 1;
        }
    }

    int crc_length = snprintf(frame + frame_length,
                              sizeof(frame) - (size_t)frame_length,
                              "%04X", crc);
    if (crc_length != 4) {
        return false;
    }

    packet->length = (size_t)frame_length + 4;
    memcpy(packet->data, frame, packet->length);
    return true;
}

bool Send_Data_APP = false;

static void app_command_processor_task(void *arg)
{
    (void)arg;

    for (;;) {
        server_packet_t incoming_packet = {0};
        if (xQueueReceive(s_server_packet_queue, &incoming_packet, portMAX_DELAY) != pdTRUE) {
            continue;
        }

        if (incoming_packet.length < 28) {
            ESP_LOGW(TAG, "Dropping undersized packet (%zu bytes)", incoming_packet.length);
            continue;
        }

        ESP_LOGI(TAG, "Received packet length=%zu", incoming_packet.length);

        uint8_t received_version[PRODUCT_VERSION_SIZE] = {0};
        for (int i = 0; i < PRODUCT_VERSION_SIZE; i++) {
            char hex[3] = {
                incoming_packet.data[2 + i * 2],
                incoming_packet.data[3 + i * 2],
                '\0'
            };
            if (hex[0] == '\0' || hex[1] == '\0') {
                ESP_LOGW(TAG, "Dropping malformed version field");
                break;
            }
            received_version[i] = (uint8_t)strtoul(hex, NULL, 16);
        }

        if (memcmp(PRODUCT_VERSION, received_version, PRODUCT_VERSION_SIZE) == 0) {
            ESP_LOGI(TAG, "Product version matched");
            uint8_t Destination = (uint8_t)strtol((char[]){incoming_packet.data[18], incoming_packet.data[19], '\0'}, NULL, 16);
            uint8_t Source = (uint8_t)strtol((char[]){incoming_packet.data[20], incoming_packet.data[21], '\0'}, NULL, 16);
            uint8_t PacketType = (uint8_t)strtol((char[]){incoming_packet.data[22], incoming_packet.data[23], '\0'}, NULL, 16);
            uint8_t Table_NO = (uint8_t)strtol((char[]){incoming_packet.data[24], incoming_packet.data[25], '\0'}, NULL, 16);
            ESP_LOGI(TAG, "Destination: %02X, Source: %02X, Packet Type: %02X, Table NO: %02X", Destination, Source, PacketType, Table_NO);

            if (PacketType == 1) {
                app_command_t command = {
                    .type = APP_COMMAND_DISPLAY_REQUEST,
                    .packet_type = PacketType,
                    .request_id = s_next_request_id++,
                    .version = {0},
                    .table_number = {0},
                };

                snprintf(command.version, sizeof(command.version),
                         "%02X%02X%02X%02X%02X%02X%02X%02X",
                         received_version[0], received_version[1],
                         received_version[2], received_version[3],
                         received_version[4], received_version[5],
                         received_version[6], received_version[7]);
                snprintf(command.table_number, sizeof(command.table_number),
                         "%02X", Table_NO);

                if (xQueueSend(s_app_command_queue, &command, 0) != pdTRUE) {
                    ESP_LOGE(TAG, "Could not queue app command");
                } else {
                    ESP_LOGI(TAG, "App command queued successfully: req=%lu, table=%s",
                             (unsigned long)command.request_id,
                             command.table_number);
                }
            }
        } else {
            ESP_LOGE(TAG, "Product version mismatch");
            vTaskDelay(pdMS_TO_TICKS(10));
            continue;
        }

        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

static bool copy_json_string(const cJSON *root, const char *name,
                             char *destination, size_t destination_size)
{
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(root, name);
    if (!cJSON_IsString(item) || item->valuestring == NULL) {
        return false;
    }

    size_t length = strlen(item->valuestring);
    if (length == 0 || length >= destination_size) {
        return false;
    }

    memcpy(destination, item->valuestring, length + 1);
    return true;
}

static bool valid_app_id(const char *app_id)
{
    for (const unsigned char *cursor = (const unsigned char *)app_id;
         *cursor; cursor++) {
        if (!isalnum(*cursor) && *cursor != '-' && *cursor != '_') {
            return false;
        }
    }
    return app_id[0] != '\0';
}

static esp_err_t save_config(const device_config_t *config)
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open("solarconnect", NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        return err;
    }

    err = nvs_set_str(handle, "ssid", config->ssid);
    if (err == ESP_OK) {
        err = nvs_set_str(handle, "password", config->password);
    }
    if (err == ESP_OK) {
        err = nvs_set_str(handle, "app_id", config->app_id);
    }
    if (err == ESP_OK) {
        err = nvs_commit(handle);
    }

    nvs_close(handle);
    return err;
}

static bool load_config(device_config_t *config)
{
    nvs_handle_t handle;
    if (nvs_open("solarconnect", NVS_READONLY, &handle) != ESP_OK) {
        return false;
    }

    size_t ssid_size = sizeof(config->ssid);
    size_t password_size = sizeof(config->password);
    size_t app_id_size = sizeof(config->app_id);

    esp_err_t err = nvs_get_str(handle, "ssid", config->ssid, &ssid_size);
    if (err == ESP_OK) {
        err = nvs_get_str(handle, "password", config->password, &password_size);
    }
    if (err == ESP_OK) {
        err = nvs_get_str(handle, "app_id", config->app_id, &app_id_size);
    }

    nvs_close(handle);

    return err == ESP_OK &&
           config->ssid[0] != '\0' &&
           config->password[0] != '\0' &&
           valid_app_id(config->app_id);
}

static esp_err_t read_request_body(httpd_req_t *request,
                                   char *body, size_t capacity)
{
    if (request->content_len == 0 ||
        (size_t)request->content_len >= capacity) {
        return ESP_ERR_INVALID_SIZE;
    }

    size_t received = 0;
    while (received < (size_t)request->content_len) {
        int result = httpd_req_recv(
            request,
            body + received,
            (size_t)request->content_len - received
        );

        if (result <= 0) {
            return ESP_FAIL;
        }
        received += (size_t)result;
    }

    body[received] = '\0';
    return ESP_OK;
}

static esp_err_t provision_handler(httpd_req_t *request)
{
    ESP_LOGI(TAG, "HTTP request received: POST %s, content length=%d",
             request->uri, request->content_len);

    char body[PROVISIONING_MAX_BODY + 1];
    esp_err_t err = read_request_body(request, body, sizeof(body));
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Could not read provisioning request body: %s",
                 esp_err_to_name(err));
        httpd_resp_send_err(request, HTTPD_400_BAD_REQUEST,
                            "Invalid request body");
        return ESP_OK;
    }

    cJSON *root = cJSON_Parse(body);
    device_config_t new_config = {0};

    bool valid =
        root != NULL &&
        copy_json_string(root, "ssid", new_config.ssid,
                         sizeof(new_config.ssid)) &&
        copy_json_string(root, "password", new_config.password,
                         sizeof(new_config.password)) &&
        copy_json_string(root, "appId", new_config.app_id,
                         sizeof(new_config.app_id)) &&
        strlen(new_config.ssid) <= 32 &&
        strlen(new_config.password) >= 8 &&
        strlen(new_config.password) <= 63 &&
        valid_app_id(new_config.app_id);

    cJSON_Delete(root);

    if (!valid) {
        ESP_LOGW(TAG, "Invalid provisioning JSON or fields");
        httpd_resp_send_err(request, HTTPD_400_BAD_REQUEST,
                            "Invalid Wi-Fi details or appId");
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Provision request: ssid='%s', appId='%s'",
             new_config.ssid, new_config.app_id);

    err = save_config(&new_config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Could not save provisioning details: %s",
                 esp_err_to_name(err));
        httpd_resp_send_err(request, HTTPD_500_INTERNAL_SERVER_ERROR,
                            "Could not save settings");
        return ESP_OK;
    }

    wifi_config_t station_config = {0};
    memcpy(station_config.sta.ssid, new_config.ssid,
           strlen(new_config.ssid));
    memcpy(station_config.sta.password, new_config.password,
           strlen(new_config.password));
    station_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;

    atomic_store(&s_has_credentials, false);
    esp_wifi_disconnect();
    vTaskDelay(pdMS_TO_TICKS(200));

    err = esp_wifi_set_config(WIFI_IF_STA, &station_config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Could not configure station: %s",
                 esp_err_to_name(err));
        httpd_resp_send_err(request, HTTPD_500_INTERNAL_SERVER_ERROR,
                            "Could not configure Wi-Fi");
        return ESP_OK;
    }

    s_device_config = new_config;
    atomic_store(&s_has_credentials, true);

    httpd_resp_set_type(request, "application/json");
    httpd_resp_set_status(request, "202 Accepted");

    char response_body[96];

    int length = snprintf(
        response_body,
        sizeof(response_body),
        "{\"status\":\"accepted\",\"deviceId\":\"%s\"}",
        s_device_id
    );

    if (length < 0 || length >= sizeof(response_body)) {
        httpd_resp_send_err(request, HTTPD_500_INTERNAL_SERVER_ERROR,
                            "Could not create response");
        return ESP_OK;
    }

    err = httpd_resp_send(
        request,
        response_body,
        HTTPD_RESP_USE_STRLEN
    );

    if (err == ESP_OK) {
        esp_err_t connect_err = esp_wifi_connect();
        if (connect_err != ESP_OK) {
            ESP_LOGW(TAG, "Initial Wi-Fi connect request failed: %s",
                     esp_err_to_name(connect_err));
        } else {
            ESP_LOGI(TAG, "Saved Wi-Fi details; joining configured network");
        }
    }

    return err;
}

static esp_err_t start_provisioning_server(void)
{
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = 80;

    esp_err_t err = httpd_start(&s_http_server, &config);
    if (err != ESP_OK) {
        return err;
    }

    httpd_uri_t provision_uri = {
        .uri = "/provision",
        .method = HTTP_POST,
        .handler = provision_handler,
        .user_ctx = NULL,
    };

    err = httpd_register_uri_handler(s_http_server, &provision_uri);
    if (err != ESP_OK) {
        httpd_stop(s_http_server);
        s_http_server = NULL;
    }

    return err;
}

static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data)
{
    (void)arg;

    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_AP_START) {
        ESP_LOGI(TAG, "Setup Wi-Fi is ready: SSID=%s", SETUP_AP_SSID);
    } else if (event_base == WIFI_EVENT &&
               event_id == WIFI_EVENT_STA_DISCONNECTED) {
        wifi_event_sta_disconnected_t *event =
            (wifi_event_sta_disconnected_t *)event_data;

        ESP_LOGW(TAG, "Wi-Fi disconnected, reason=%d", event->reason);
        xEventGroupClearBits(s_wifi_events, WIFI_CONNECTED_BIT);

        if (atomic_load(&s_has_credentials)) {
            esp_err_t err = esp_wifi_connect();
            if (err != ESP_OK) {
                ESP_LOGW(TAG, "Reconnect request failed: %s",
                         esp_err_to_name(err));
            }
        }
    } else if (event_base == IP_EVENT &&
               event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;

        ESP_LOGI(TAG, "Joined Wi-Fi; IP=" IPSTR " gateway=" IPSTR,
                 IP2STR(&event->ip_info.ip),
                 IP2STR(&event->ip_info.gw));

        xEventGroupSetBits(s_wifi_events, WIFI_CONNECTED_BIT);
    }
}

static bool send_all(int socket_fd, const char *data, size_t length)
{
    size_t sent = 0;

    while (sent < length) {
        int result = send(socket_fd, data + sent, length - sent, 0);

        if (result < 0 && errno == EINTR) {
            continue;
        }
        if (result <= 0) {
            return false;
        }

        sent += (size_t)result;
    }

    return true;
}

static bool receive_line(int socket_fd, char *line, size_t capacity,
                         uint32_t timeout_ms)
{
    size_t used = 0;
    int64_t deadline = esp_timer_get_time() + (int64_t)timeout_ms * 1000;

    while (used + 1 < capacity) {
        int64_t remaining = deadline - esp_timer_get_time();
        if (remaining <= 0) return false;

        fd_set read_set;
        FD_ZERO(&read_set);
        FD_SET(socket_fd, &read_set);

        struct timeval timeout = {
            .tv_sec = remaining / 1000000,
            .tv_usec = remaining % 1000000,
        };

        int ready = select(socket_fd + 1, &read_set, NULL, NULL, &timeout);

        if (ready < 0 && errno == EINTR) continue;
        if (ready <= 0) return false;

        char ch;
        int received = recv(socket_fd, &ch, 1, 0);
        if (received <= 0) return false;

        if (ch == '\n') {
            if (used > 0 && line[used - 1] == '\r') used--;
            line[used] = '\0';
            return true;
        }

        line[used++] = ch;
    }

    return false;
}


/*
 * On a phone hotspot, the phone is commonly the ESP32's gateway.
 * This keeps the existing hotspot connection path working.
 */
static uint32_t get_gateway_ip(void)
{
    esp_netif_ip_info_t ip_info;

    if (esp_netif_get_ip_info(s_sta_netif, &ip_info) != ESP_OK) {
        return 0;
    }

    return ip_info.gw.addr;
}

/*
 * Find the phone's advertised _solarconnect._tcp service.
 * The phone app must publish this service on TCP port 8080.
 */
static bool resolve_phone_ip(uint32_t *phone_ip)
{
    mdns_result_t *services = NULL;

    esp_err_t err = mdns_query_ptr(
        "_solarconnect",
        "_tcp",
        MDNS_QUERY_TIMEOUT_MS,
        5,
        &services
    );

    if (err != ESP_OK || services == NULL) {
        ESP_LOGW(TAG, "mDNS service _solarconnect._tcp not found: %s",
                 esp_err_to_name(err));
        mdns_query_results_free(services);
        return false;
    }

    const char *instance_name = services->instance_name;
    if (instance_name == NULL) {
        ESP_LOGW(TAG, "mDNS service result has no instance name");
        mdns_query_results_free(services);
        return false;
    }

    mdns_result_t *service = NULL;
    err = mdns_query_srv(
        instance_name,
        "_solarconnect",
        "_tcp",
        MDNS_QUERY_TIMEOUT_MS,
        &service
    );

    mdns_query_results_free(services);

    if (err != ESP_OK || service == NULL ||
        service->hostname == NULL ||
        service->port != APP_SERVER_PORT) {
        ESP_LOGW(TAG, "Could not resolve service target or port: %s",
                 esp_err_to_name(err));
        mdns_query_results_free(service);
        return false;
    }

    esp_ip4_addr_t address = {0};
    err = mdns_query_a(
        service->hostname,
        MDNS_QUERY_TIMEOUT_MS,
        &address
    );

    if (err != ESP_OK || address.addr == 0) {
        ESP_LOGW(TAG, "Could not resolve mDNS target %s: %s",
                 service->hostname, esp_err_to_name(err));
        mdns_query_results_free(service);
        return false;
    }

    *phone_ip = address.addr;

    char ip_text[INET_ADDRSTRLEN] = {0};
    inet_ntop(AF_INET, phone_ip, ip_text, sizeof(ip_text));

    ESP_LOGI(TAG, "mDNS service resolved to %s:%d",
             ip_text, service->port);

    mdns_query_results_free(service);
    return true;
}

static bool run_tcp_connection(uint32_t phone_ip)
{
    device_config_t config = s_device_config;   // Copy the device configuration
    int socket_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_IP); // Create a network socket
    if (socket_fd < 0) { // Negative value indicates socket failure
        ESP_LOGE(TAG, "Could not create TCP socket: errno=%d", errno);
        return false;
    }

    struct sockaddr_in server_address = { // Creates the destination address
        .sin_family = AF_INET,
        .sin_port = htons(APP_SERVER_PORT),
        .sin_addr.s_addr = phone_ip,
    };

    // Converts the binary IPv4 address into text like "192.168.1.25"
    char phone[INET_ADDRSTRLEN] = {0};
    inet_ntop(AF_INET, &server_address.sin_addr, phone, sizeof(phone));
    // Log the Phone address and port the ESP32 is about to connect to
    ESP_LOGI(TAG, "Connecting to phone TCP server at %s:%d", phone, APP_SERVER_PORT);

    // Attempts to establish the TCP connection
    if (connect(socket_fd, (struct sockaddr *)&server_address, sizeof(server_address)) != 0) {
        ESP_LOGW(TAG, "TCP connect failed: errno=%d", errno);
        close(socket_fd);
        return false;
    }

    //===========[Send the Identity]=====================
    char greeting[128];
    // Fomats the greeting
    int greeting_length = snprintf(greeting, sizeof(greeting), "HELLO:%s:%s\n", config.app_id, s_device_id);
    // Using the send_all() function the greeting is sent
    if (greeting_length <= 0 ||
        (size_t)greeting_length >= sizeof(greeting) ||
        !send_all(socket_fd, greeting, (size_t)greeting_length)) {
        ESP_LOGW(TAG, "Could not send the app ID greeting");
        close(socket_fd);
        return false;
    }

    //==============[Receive and Check the Authentication]=========
    char response[128];
    // Receive the data from phone 
    if(!receive_line(socket_fd, response, sizeof(response), 5000)) {
        // Wait for newline and max wait is 5 sec.
        ESP_LOGW(TAG, "Could not send identity greeting");
        close(socket_fd);
        return false;
    }

    char expected_response[128];
    int expected_length = snprintf(expected_response,sizeof(expected_response),"AUTH_OK:%s:%s",config.app_id, s_device_id);

    // Check if the response is valid and matches the expected response
    if (expected_length <= 0 ||(size_t)expected_length >= sizeof(expected_response) ||strcmp(response, expected_response) != 0) {
        ESP_LOGW(TAG, "App identity check failed; connection rejected");
        close(socket_fd);
        return false;
    }

    ESP_LOGI(TAG, "App and ESP32 identities verified");

    char receive_buffer[SERVER_PACKET_MAX_SIZE * 2];
    size_t receive_length = 0;
    // bool request_pending = false;
    // uint32_t pending_request_id = 0;
    // server_packet_t pending_packet = {0};
    // bool awaiting_ack = false;
    // uint8_t retry_count = 0;
    // int64_t ack_deadline = 0;
    bool resend_requested = false;
    bool connected = true;

    while (connected) {
        app_response_t outbound_response = {0};
        if (xQueueReceive(s_app_response_queue, &outbound_response, 0) == pdTRUE) {
            if (outbound_response.packet.length > 0 &&
                outbound_response.packet.length <= sizeof(outbound_response.packet.data)) {
                if (!send_all(socket_fd, (const char *)outbound_response.packet.data,
                              outbound_response.packet.length)) {
                    ESP_LOGW(TAG, "Could not send queued response packet to phone");
                    connected = false;
                    break;
                }
                ESP_LOGI(TAG, "Sent queued response packet (%zu bytes)",
                         outbound_response.packet.length);
            } else {
                ESP_LOGW(TAG, "Queued response has invalid length: %zu",
                         outbound_response.packet.length);
            }
        }

        fd_set read_set;
        FD_ZERO(&read_set);
        FD_SET(socket_fd, &read_set);

        struct timeval timeout = {.tv_sec = 0, .tv_usec = 20000};
        int ready = select(socket_fd + 1, &read_set, NULL, NULL, &timeout);

        if (ready < 0 && errno != EINTR) {
            ESP_LOGW(TAG, "TCP select failed: errno=%d", errno);
            break;
        }

        if (ready > 0 && FD_ISSET(socket_fd, &read_set)) {
            char incoming[128];
            int received = recv(socket_fd, incoming, sizeof(incoming), 0); // Receive the data from the app

            if (received <= 0) {
                ESP_LOGI(TAG, "Phone closed the TCP connection");
                break;
            }

            if ((size_t)received > sizeof(receive_buffer) - receive_length) {
                ESP_LOGE(TAG, "TCP receive buffer overflow; closing connection");
                connected = false;
                break;
            }

            memcpy(receive_buffer + receive_length, incoming, (size_t)received);
            receive_length += (size_t)received;
        }

        while (connected && receive_length > 0) {
            static const char ping[] = "PING\n";

            if (receive_length >= sizeof(ping) - 1 && memcmp(receive_buffer, ping, sizeof(ping) - 1) == 0) {
                static const char pong[] = "PONG\n";
                if (!send_all(socket_fd, pong, sizeof(pong) - 1)) {
                    ESP_LOGW(TAG, "Could not send heartbeat response: PONG");
                    connected = false;
                    break;
                }
                ESP_LOGI(TAG, "Received heartbeat request: PING; sent PONG");
                memmove(receive_buffer,
                        receive_buffer + sizeof(ping) - 1,
                        receive_length - (sizeof(ping) - 1));
                receive_length -= sizeof(ping) - 1;
                continue;
            }

            if (receive_length < 2) {
                break;
            }


            if (receive_buffer[0] == '#' && receive_buffer[1] == '#') {
                ESP_LOGI(TAG, "receive_buffer raw: len=%zu data=", receive_length);
                if (receive_length < 8) {
                    ESP_LOGW(TAG, "Discarding short packet: len=%zu", receive_length);
                    memset(receive_buffer, 0, sizeof(receive_buffer));
                    receive_length = 0;
                    vTaskDelay(pdMS_TO_TICKS(10));
                    continue;
                }

                uint16_t calculated_crc = 0xFFFF;
                for (size_t i = 0; i < receive_length - 4; i++) {
                    calculated_crc ^= (uint8_t)receive_buffer[i];
                    for (uint8_t bit = 0; bit < 8; bit++) {
                        calculated_crc = (calculated_crc & 1U)
                            ? (calculated_crc >> 1) ^ 0xA001U
                            : calculated_crc >> 1;
                    }
                }

                if (receive_length < 4) {
                    ESP_LOGW(TAG, "Packet too short for CRC: len=%zu", receive_length);
                    memset(receive_buffer, 0, sizeof(receive_buffer));
                    receive_length = 0;
                    vTaskDelay(pdMS_TO_TICKS(10));
                    continue;
                }

                uint16_t received_crc = (uint16_t)strtoul(
                    (char[]){
                        receive_buffer[receive_length - 4],
                        receive_buffer[receive_length - 3],
                        receive_buffer[receive_length - 2],
                        receive_buffer[receive_length - 1],
                        '\0'
                    },
                    NULL,
                    16
                );
                if (calculated_crc == received_crc) {
                    ESP_LOGI(TAG, "CRC Value: %04X", calculated_crc);
                    server_packet_t packet = {0};
                    packet.length = receive_length;
                    memcpy(packet.data, receive_buffer, receive_length);
                    if (xQueueSend(s_server_packet_queue, &packet, 0) == pdTRUE) {
                        memset(receive_buffer, 0, sizeof(receive_buffer));
                        receive_length = 0;
                    } else {
                        ESP_LOGW(TAG, "Queue full; dropped packet");
                        memset(receive_buffer, 0, sizeof(receive_buffer));
                        receive_length = 0;
                    }
                } else {
                    ESP_LOGW(TAG, "CRC mismatch: calc=%04X recv=%04X; dropping packet",
                             calculated_crc, received_crc);
                    memset(receive_buffer, 0, sizeof(receive_buffer));
                    receive_length = 0;
                    vTaskDelay(pdMS_TO_TICKS(10));
                }
            } else {
                ESP_LOGW(TAG, "Discarding non-APP packet prefix; len=%zu", receive_length);
                memset(receive_buffer, 0, sizeof(receive_buffer));
                receive_length = 0;
                vTaskDelay(pdMS_TO_TICKS(10));
            }
        }
    }

    shutdown(socket_fd, SHUT_RDWR);
    close(socket_fd);
    return true;
}

static void tcp_client_task(void *arg)
{
    (void)arg;

    for (;;) {
        xEventGroupWaitBits(
            s_wifi_events,
            WIFI_CONNECTED_BIT,
            pdFALSE,
            pdTRUE,
            portMAX_DELAY
        );

        if (!atomic_load(&s_has_credentials)) {
            continue;
        }

        /*
         * Keep the existing phone-hotspot attempt.
         * On a phone hotspot, the phone is normally the gateway.
         */
        // uint32_t gateway = get_gateway_ip();
        // if (gateway != 0 && run_tcp_connection(gateway)) {
        //     vTaskDelay(pdMS_TO_TICKS(TCP_RETRY_DELAY_MS));
        //     continue;
        // }

        /*
         * Router Wi-Fi path: do a fresh mDNS lookup for every retry.
         * The resolved phone IP is not saved between attempts.
         */
        uint32_t phone_ip = 0;
        if (resolve_phone_ip(&phone_ip)) {
            run_tcp_connection(phone_ip);
        }

        vTaskDelay(pdMS_TO_TICKS(TCP_RETRY_DELAY_MS));
    }
}

static void start_wifi(void)
{
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    esp_netif_create_default_wifi_ap();
    s_sta_netif = esp_netif_create_default_wifi_sta();

    ESP_ERROR_CHECK(mdns_init());

    wifi_init_config_t init_config = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&init_config));

    ESP_ERROR_CHECK(esp_event_handler_register(
        WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_event_handler, NULL
    ));
    ESP_ERROR_CHECK(esp_event_handler_register(
        IP_EVENT, IP_EVENT_STA_GOT_IP, wifi_event_handler, NULL
    ));

    wifi_config_t ap_config = {0};
    memcpy(ap_config.ap.ssid, SETUP_AP_SSID, sizeof(SETUP_AP_SSID));
    memcpy(ap_config.ap.password, SETUP_AP_PASSWORD,
           sizeof(SETUP_AP_PASSWORD));

    ap_config.ap.ssid_len = strlen(SETUP_AP_SSID);
    ap_config.ap.channel = 1;
    ap_config.ap.max_connection = 4;
    ap_config.ap.authmode = WIFI_AUTH_WPA2_PSK;

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &ap_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    device_config_t saved_config = {0};
    if (load_config(&saved_config)) {
        wifi_config_t station_config = {0};

        memcpy(station_config.sta.ssid, saved_config.ssid,
               strlen(saved_config.ssid));
        memcpy(station_config.sta.password, saved_config.password,
               strlen(saved_config.password));
        station_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;

        s_device_config = saved_config;
        atomic_store(&s_has_credentials, true);

        ESP_ERROR_CHECK(
            esp_wifi_set_config(WIFI_IF_STA, &station_config)
        );
        ESP_ERROR_CHECK(esp_wifi_connect());
    }
}

void app_main(void)
{
    esp_err_t err = nvs_flash_init();   // Initialize NVS

    if (err == ESP_ERR_NVS_NO_FREE_PAGES ||
        err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);
    init_device_id() ;

    s_wifi_events = xEventGroupCreate();
    if (s_wifi_events == NULL) {
        ESP_LOGE(TAG, "Could not create the Wi-Fi event group");
        return;
    }

    // Server Packet Queue: This queue is used to hold packets that are received from the server.
    s_server_packet_queue = xQueueCreate( //for Data Receiving from Server
        SERVER_PACKET_QUEUE_LENGTH, sizeof(server_packet_t)
    );
    if (s_server_packet_queue == NULL) {
        ESP_LOGE(TAG, "Could not create telemetry packet queue");
        return;
    }

    s_app_command_queue = xQueueCreate(
        APP_COMMAND_QUEUE_LENGTH, sizeof(app_command_t)
    );
    if (s_app_command_queue == NULL) {
        ESP_LOGE(TAG, "Could not create app command queue");
        return;
    }

    // Holds the responses of process data from esp32
    s_app_response_queue = xQueueCreate(
        APP_COMMAND_QUEUE_LENGTH, sizeof(server_packet_t)
    );
    if (s_app_response_queue == NULL) {
        ESP_LOGE(TAG, "Could not create app response queue");
        return;
    }

    start_wifi();
    ESP_ERROR_CHECK(start_provisioning_server());

    if (xTaskCreate(app_command_processor_task, "app_command",
                    4096, NULL, 5, NULL) != pdPASS) {
        ESP_LOGE(TAG, "Could not start app command processor task");
        return;
    }

    if (xTaskCreate(tcp_client_task, "tcp_client",
                    4096, NULL, 5, NULL) != pdPASS) {
        ESP_LOGE(TAG, "Could not start TCP client task");
    }

    if (xTaskCreate(Process_data, "process_data",
                    8192, NULL, 5, NULL) != pdPASS) {
        ESP_LOGE(TAG, "Could not start packet processing task");
    }
}
