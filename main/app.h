


#ifndef APP_H
#define APP_H

#include "wifi.h"
#include "com.h"
#include "rtc.h"




typedef struct {
    char ssid[33];
    char password[64];
    char app_id[65];
} device_config_t;

extern const char *TAG;
extern EventGroupHandle_t s_wifi_events;
extern esp_netif_t *s_sta_netif;
extern httpd_handle_t s_http_server;
extern device_config_t s_device_config;
extern atomic_bool s_has_credentials;
extern QueueHandle_t s_server_packet_queue;
extern QueueHandle_t s_app_command_queue;
extern QueueHandle_t s_app_response_queue;
extern char s_device_id[32];
extern uint32_t s_next_request_id;



// Function prototypes for internal functions
void start_wifi(void);
void init_device_id(void);
bool load_config(device_config_t *config);
void app_command_processor_task(void *arg);
void wifi_event_handler(void *arg, esp_event_base_t event_base,
                        int32_t event_id, void *event_data);

bool copy_json_string(const cJSON *root, const char *name,
                             char *destination, size_t destination_size);

esp_err_t save_config(const device_config_t *config);
bool valid_app_id(const char *app_id);

esp_err_t start_provisioning_server(void);
esp_err_t provision_handler(httpd_req_t *request);

esp_err_t read_request_body(httpd_req_t *request, char *body, size_t capacity);


void tcp_client_task(void *arg);
bool run_tcp_connection(uint32_t phone_ip);
bool resolve_phone_ip(uint32_t *phone_ip);
uint32_t get_gateway_ip(void);
bool send_all(int socket_fd, const char *data, size_t length);
bool receive_line(int socket_fd, char *line, size_t capacity,uint32_t timeout_ms);
void wifi_event_handler(void *arg, esp_event_base_t event_base,int32_t event_id, void *event_data);

#endif