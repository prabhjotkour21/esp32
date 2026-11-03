
#include "app.h"
#include "com.h"

// Shared application state. These handles and values are initialized by app_main
// or the startup functions below, then used by the Wi-Fi and communication tasks.

const char *TAG = "solarconnect";                    // Prefix application log messages with this name.
EventGroupHandle_t s_wifi_events;                    // Signals when station Wi-Fi gains or loses connectivity.
esp_netif_t *s_sta_netif;                            // Handle for the station network interface.
httpd_handle_t s_http_server;                        // Handle used to manage the provisioning HTTP server.
device_config_t s_device_config;                     // Active SSID, password, and phone-app ID.
atomic_bool s_has_credentials = ATOMIC_VAR_INIT(false); // Shared flag safe to read/write across tasks.
QueueHandle_t s_server_packet_queue;                 // Carries phone packets to the packet processor.
QueueHandle_t s_app_command_queue;                   // Carries decoded requests to application processing.
QueueHandle_t s_app_response_queue;                  // Carries responses back to the TCP sender.
char s_device_id[32];                                // Persistent identity sent to the phone app.
uint32_t s_next_request_id;                           // ID assigned to each generated app command.



// Load the device ID from NVS, or persist the default ID on first boot.
void init_device_id(void)
{
    nvs_handle_t handle; // NVS handle used to access the device's persistent settings.

    //Existing "solarconnect" NVS namespace 
    ESP_ERROR_CHECK(nvs_open("solarconnect",NVS_READWRITE, &handle)); // Open/create the namespace with read/write access.

    size_t length =sizeof(s_device_id); // Tell NVS the capacity of the destination buffer.

    esp_err_t err =nvs_get_str(handle,"device_id",s_device_id, &length); // Read the stored ID into the global buffer.

    if(err==ESP_ERR_NVS_NOT_FOUND){
        // No ID exists yet, so initialize this board with the configured default.
       
        snprintf(s_device_id,sizeof(s_device_id),"%s",
            DEVICE_ID); // Copy the default while respecting the buffer capacity.

        ESP_ERROR_CHECK(nvs_set_str(handle,"device_id",s_device_id)); // Stage the new ID in NVS.
        ESP_ERROR_CHECK(nvs_commit(handle)); // Persist the staged value to flash.
    } else {
        // Treat any result other than "not found" as success or a real NVS error.
        ESP_ERROR_CHECK(err);
    }
    nvs_close(handle) ; // Release the NVS handle after the read/write is complete.

    ESP_LOGI(TAG,"Device ID: %s",s_device_id); // Display the ID in the ESP-IDF log.

}


bool Send_Data_APP = false;

// Consume validated network packets and turn display requests into app commands.
// This task blocks on the packet queue, so it sleeps until a packet arrives.
void app_command_processor_task(void *arg)
{
    (void)arg; // This task does not need the optional FreeRTOS task argument.

    for (;;) {
        server_packet_t incoming_packet = {0}; // Clear storage before receiving one queued packet.
        if (xQueueReceive(s_server_packet_queue, &incoming_packet, portMAX_DELAY) != pdTRUE) { // Wait indefinitely for network input.
            continue;
        }

        if (incoming_packet.length < 28) { // The fixed header cannot be decoded if the frame is shorter.
            ESP_LOGW(TAG, "Dropping undersized packet (%zu bytes)", incoming_packet.length);
            continue;
        }

        ESP_LOGI(TAG, "Received packet length=%zu", incoming_packet.length); // Record packet size for diagnostics.

        // The packet encodes the product version as 8 bytes written in hex
        // (16 ASCII characters), beginning at byte offset 2.
        uint8_t received_version[PRODUCT_VERSION_SIZE] = {0};
        for (int i = 0; i < PRODUCT_VERSION_SIZE; i++) { // Decode each two-character hex byte of the version.
            char hex[3] = {
                incoming_packet.data[2 + i * 2],
                incoming_packet.data[3 + i * 2],
                '\0'
            };
            if (hex[0] == '\0' || hex[1] == '\0') { // Reject a missing character instead of parsing an incomplete byte.
                ESP_LOGW(TAG, "Dropping malformed version field");
                break;
            }
            received_version[i] = (uint8_t)strtoul(hex, NULL, 16); // Convert the two ASCII hex digits to one byte.
        }

        if (memcmp(PRODUCT_VERSION, received_version, PRODUCT_VERSION_SIZE) == 0) { // Process only packets for this product version.
            ESP_LOGI(TAG, "Product version matched");
            // Packet header fields are two-character hexadecimal strings.
            uint8_t Destination = (uint8_t)strtol((char[]){incoming_packet.data[18], incoming_packet.data[19], '\0'}, NULL, 16);
            uint8_t Source = (uint8_t)strtol((char[]){incoming_packet.data[20], incoming_packet.data[21], '\0'}, NULL, 16);
            uint8_t PacketType = (uint8_t)strtol((char[]){incoming_packet.data[22], incoming_packet.data[23], '\0'}, NULL, 16);
            uint8_t Table_NO = (uint8_t)strtol((char[]){incoming_packet.data[24], incoming_packet.data[25], '\0'}, NULL, 16);
            ESP_LOGI(TAG, "Destination: %02X, Source: %02X, Packet Type: %02X, Table NO: %02X", Destination, Source, PacketType, Table_NO); // Log decoded header fields.

            if (PacketType == 1) { // Type 1 represents a display request in this protocol.
                // Translate this wire-protocol request into the command format
                // consumed by the app command-processing code.
                app_command_t command = {
                    .type = APP_COMMAND_DISPLAY_REQUEST,
                    .packet_type = PacketType,
                    .request_id = s_next_request_id++, // Assign a unique local ID, then advance the counter.
                    .version = {0},
                    .table_number = {0},
                };

                snprintf(command.version, sizeof(command.version), // Format the version bytes back into the command's hex-string field.
                         "%02X%02X%02X%02X%02X%02X%02X%02X",
                         received_version[0], received_version[1],
                         received_version[2], received_version[3],
                         received_version[4], received_version[5],
                         received_version[6], received_version[7]);
                snprintf(command.table_number, sizeof(command.table_number), // Format the table number as a two-character hex value.
                         "%02X", Table_NO);
                printf("Sending command queue");
                if (xQueueSend(s_app_command_queue, &command, 0) != pdTRUE) { // Use a non-blocking enqueue so this task stays responsive.
                    ESP_LOGE(TAG, "Could not queue app command");
                } else { // A different version means the packet is not for this firmware.
                    ESP_LOGI(TAG, "App command queued successfully: req=%lu, table=%s",
                             (unsigned long)command.request_id,
                             command.table_number);
                }
            }
            else if(PacketType == 7){


                app_command_t stamp_command = {
                    .type = APP_COMMAND_TIMESTAMP_REQUEST,
                    .packet_type = PacketType,
                    .request_id = s_next_request_id++,
                    .version = {0},
                    .table_number = {0},
                    .timestamp = 0
                };

                stamp_command.timestamp =
                    ((uint32_t)getByte(&incoming_packet.data[P_HEADER_SIZE]) << 24) |
                    ((uint32_t)getByte(&incoming_packet.data[P_HEADER_SIZE + 2]) << 16) |
                    ((uint32_t)getByte(&incoming_packet.data[P_HEADER_SIZE + 4]) << 8) |
                    (uint32_t)getByte(&incoming_packet.data[P_HEADER_SIZE + 6]);

                if (xQueueSend(s_app_command_queue,&stamp_command,0) != pdTRUE) {
                    printf("Timestamp command could not be queued\n");
                }
                else{
                    printf("Timestamp command queued successfully\n");
                }

            }
        } else {
            ESP_LOGE(TAG, "Product version mismatch");
            vTaskDelay(pdMS_TO_TICKS(10)); // Yield briefly to allow other FreeRTOS tasks to run.
            continue;
        }
        vTaskDelay(pdMS_TO_TICKS(2));
        // vTaskDelay(pdMS_TO_TICKS(10)); // Avoid monopolizing the CPU between packets.
    }
}







// Initialize both Wi-Fi interfaces: the setup AP accepts credentials while the
// station interface joins the user's network when saved credentials exist.
void start_wifi(void)
{
    ESP_ERROR_CHECK(esp_netif_init()); // Initialize ESP-IDF's network interface layer.
    ESP_ERROR_CHECK(esp_event_loop_create_default()); // Create the default event loop for Wi-Fi/IP events.

    esp_netif_create_default_wifi_ap(); // Create the interface for the device's setup access point.
    s_sta_netif = esp_netif_create_default_wifi_sta(); // Create and save the interface for joining a router.

    ESP_ERROR_CHECK(mdns_init()); // Initialize mDNS so the phone can be found by service name.

    wifi_init_config_t init_config = WIFI_INIT_CONFIG_DEFAULT(); // Start with Espressif's standard Wi-Fi driver settings.
    ESP_ERROR_CHECK(esp_wifi_init(&init_config)); // Initialize the Wi-Fi driver.

    // Receive AP/station state changes and the station's assigned IP address.
    ESP_ERROR_CHECK(esp_event_handler_register(
        WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_event_handler, NULL
    ));
    ESP_ERROR_CHECK(esp_event_handler_register(
        IP_EVENT, IP_EVENT_STA_GOT_IP, wifi_event_handler, NULL
    ));

    // Configure the local access point used for first-time provisioning.
    wifi_config_t ap_config = {0};
    memcpy(ap_config.ap.ssid, SETUP_AP_SSID, sizeof(SETUP_AP_SSID));
    memcpy(ap_config.ap.password, SETUP_AP_PASSWORD,
           sizeof(SETUP_AP_PASSWORD));

    ap_config.ap.ssid_len = strlen(SETUP_AP_SSID); // Provide the SSID length without its terminating NUL.
    ap_config.ap.channel = 1; // Select the access-point radio channel.
    ap_config.ap.max_connection = 4; // Allow up to four clients on the setup AP.
    ap_config.ap.authmode = WIFI_AUTH_WPA2_PSK; // Require WPA2 password authentication.

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA)); // Run setup AP and router station at the same time.
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &ap_config)); // Apply the setup AP settings.
    ESP_ERROR_CHECK(esp_wifi_start()); // Start the Wi-Fi driver and AP.

    // Reuse previously provisioned credentials, if available.
    device_config_t saved_config = {0};
    if (load_config(&saved_config)) { // A valid saved configuration allows automatic station reconnection.
        wifi_config_t station_config = {0};

        memcpy(station_config.sta.ssid, saved_config.ssid,
               strlen(saved_config.ssid));
        memcpy(station_config.sta.password, saved_config.password,
               strlen(saved_config.password));
        station_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK; // Require at least WPA2 when selecting the network.

        s_device_config = saved_config;
        atomic_store(&s_has_credentials, true); // Let reconnect logic know credentials are ready.

        ESP_ERROR_CHECK(
            esp_wifi_set_config(WIFI_IF_STA, &station_config)
        );
        ESP_ERROR_CHECK(esp_wifi_connect());
    }
}

// Copy a JSON string only if it exists, is non-empty, and fits with its
// terminating NUL byte in the destination buffer.
bool copy_json_string(const cJSON *root, const char *name,
                             char *destination, size_t destination_size)
{
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(root, name);
    if (!cJSON_IsString(item) || item->valuestring == NULL) { // Reject missing fields and JSON values of the wrong type.
        return false;
    }

    size_t length = strlen(item->valuestring); // Measure the source string before copying it.
    if (length == 0 || length >= destination_size) { // Require non-empty input and room for its NUL terminator.
        return false;
    }

    memcpy(destination, item->valuestring, length + 1); // Copy the text and its terminating NUL byte.
    return true;
}

// Restrict app IDs to URL/protocol-friendly letters, digits, '-' and '_'.
bool valid_app_id(const char *app_id)
{
    for (const unsigned char *cursor = (const unsigned char *)app_id;
         *cursor; cursor++) { // Check every byte until the string terminator.
        if (!isalnum(*cursor) && *cursor != '-' && *cursor != '_') {
            return false;
        }
    }
    return app_id[0] != '\0';
}

// Persist all provisioning fields together in the "solarconnect" NVS namespace.
esp_err_t save_config(const device_config_t *config)
{
    nvs_handle_t handle; // Handle for writing the provisioning namespace.
    esp_err_t err = nvs_open("solarconnect", NVS_READWRITE, &handle); // Open the namespace for updates.
    if (err != ESP_OK) {
        return err;
    }

    err = nvs_set_str(handle, "ssid", config->ssid); // Save the Wi-Fi network name first.
    if (err == ESP_OK) {
        err = nvs_set_str(handle, "password", config->password); // Continue only if the previous write succeeded.
    }
    if (err == ESP_OK) {
        err = nvs_set_str(handle, "app_id", config->app_id); // Save the phone application's identity.
    }
    if (err == ESP_OK) {
        err = nvs_commit(handle); // Commit staged changes to persistent storage.
    }

    nvs_close(handle); // Always release the NVS handle after opening it.
    return err; // Report whichever write/commit operation failed, or ESP_OK.
}

// Read the saved provisioning fields and reject incomplete or invalid settings.
bool load_config(device_config_t *config)
{
    nvs_handle_t handle; // Handle for reading the saved provisioning namespace.
    if (nvs_open("solarconnect", NVS_READONLY, &handle) != ESP_OK) { // Missing/unavailable namespace means no usable config.
        return false;
    }

    size_t ssid_size = sizeof(config->ssid); // Pass each destination's full capacity to NVS.
    size_t password_size = sizeof(config->password); // NVS updates these sizes with the stored string lengths.
    size_t app_id_size = sizeof(config->app_id);

    esp_err_t err = nvs_get_str(handle, "ssid", config->ssid, &ssid_size); // Read the SSID before dependent fields.
    if (err == ESP_OK) {
        err = nvs_get_str(handle, "password", config->password, &password_size); // Read the password only after SSID succeeds.
    }
    if (err == ESP_OK) {
        err = nvs_get_str(handle, "app_id", config->app_id, &app_id_size); // Read the app ID only after prior reads succeed.
    }

    nvs_close(handle); // Release the read-only NVS handle.

    return err == ESP_OK && // All three NVS reads must have succeeded.
           config->ssid[0] != '\0' &&
           config->password[0] != '\0' &&
           valid_app_id(config->app_id);
}

// Read the complete HTTP body, handling partial receives and reserving one byte
// for the NUL terminator expected by the JSON parser.
esp_err_t read_request_body(httpd_req_t *request, char *body, size_t capacity)
{
    if (request->content_len == 0 ||
        (size_t)request->content_len >= capacity) { // Reject empty bodies and bodies too large to NUL-terminate.
        return ESP_ERR_INVALID_SIZE;
    }

    size_t received = 0; // Track bytes copied from the HTTP request so far.
    while (received < (size_t)request->content_len) {
        int result = httpd_req_recv(
            request,
            body + received,
            (size_t)request->content_len - received
        );

        if (result <= 0) { // A failed/closed receive means the request is incomplete.
            return ESP_FAIL;
        }
        received += (size_t)result; // Advance by the number of bytes actually received.
    }

    body[received] = '\0'; // Make the body a C string for JSON parsing.
    return ESP_OK; // The full declared content length has been read.
}

// Handle POST /provision: validate JSON, save settings, configure station Wi-Fi,
// respond to the phone, then start connecting to the configured network.
esp_err_t provision_handler(httpd_req_t *request)
{
    ESP_LOGI(TAG, "HTTP request received: POST %s, content length=%d",
             request->uri, request->content_len);

    char body[PROVISIONING_MAX_BODY + 1]; // Reserve one extra byte for the string terminator.
    esp_err_t err = read_request_body(request, body, sizeof(body)); // Read the request before parsing it.
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Could not read provisioning request body: %s",
                 esp_err_to_name(err));
        httpd_resp_send_err(request, HTTPD_400_BAD_REQUEST,
                            "Invalid request body");
        return ESP_OK;
    }

    // Parse into a temporary configuration so invalid input cannot overwrite
    // the currently active settings.
    cJSON *root = cJSON_Parse(body); // Parse JSON; returns NULL if the body is malformed.
    device_config_t new_config = {0}; // Keep candidate settings separate until all checks pass.

    bool valid = // Short-circuit if parsing or any field/constraint check fails.
        root != NULL &&
        copy_json_string(root, "ssid", new_config.ssid,
                         sizeof(new_config.ssid)) &&
        copy_json_string(root, "password", new_config.password,
                         sizeof(new_config.password)) &&
        copy_json_string(root, "appId", new_config.app_id,
                         sizeof(new_config.app_id)) &&
        strlen(new_config.ssid) <= 32 && // Wi-Fi SSIDs can contain at most 32 bytes.
        strlen(new_config.password) >= 8 &&
        strlen(new_config.password) <= 63 &&
        valid_app_id(new_config.app_id);

    cJSON_Delete(root); // Free the JSON tree even if parsing failed (NULL is accepted).

    if (!valid) {
        ESP_LOGW(TAG, "Invalid provisioning JSON or fields");
        httpd_resp_send_err(request, HTTPD_400_BAD_REQUEST,
                            "Invalid Wi-Fi details or appId");
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Provision request: ssid='%s', appId='%s'",
             new_config.ssid, new_config.app_id);

    err = save_config(&new_config); // Persist validated credentials before applying them to the driver.
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Could not save provisioning details: %s",
                 esp_err_to_name(err));
        httpd_resp_send_err(request, HTTPD_500_INTERNAL_SERVER_ERROR,
                            "Could not save settings");
        return ESP_OK;
    }

    wifi_config_t station_config = {0}; // Zero unused driver fields before setting station credentials.
    memcpy(station_config.sta.ssid, new_config.ssid,
           strlen(new_config.ssid));
    memcpy(station_config.sta.password, new_config.password,
           strlen(new_config.password));
    station_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;

    // Prevent the reconnect handler from using old credentials while replacing
    // the station configuration.
    atomic_store(&s_has_credentials, false); // Suppress automatic reconnect during credential replacement.
    esp_wifi_disconnect(); // Disconnect from any network using the previous settings.
    vTaskDelay(pdMS_TO_TICKS(200)); // Give the driver time to process the disconnect event.

    err = esp_wifi_set_config(WIFI_IF_STA, &station_config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Could not configure station: %s",
                 esp_err_to_name(err));
        httpd_resp_send_err(request, HTTPD_500_INTERNAL_SERVER_ERROR,
                            "Could not configure Wi-Fi");
        return ESP_OK;
    }

    s_device_config = new_config; // Publish the new settings for the TCP client task.
    atomic_store(&s_has_credentials, true); // Re-enable reconnect attempts with the new credentials.

    httpd_resp_set_type(request, "application/json"); // Tell the client the response body is JSON.
    httpd_resp_set_status(request, "202 Accepted"); // Indicate acceptance; Wi-Fi joining continues asynchronously.

    char response_body[96]; // Buffer for a short JSON acknowledgement.

    int length = snprintf(
        response_body,
        sizeof(response_body),
        "{\"status\":\"accepted\",\"deviceId\":\"%s\"}",
        s_device_id
    );

    if (length < 0 || length >= sizeof(response_body)) { // Detect formatting errors or truncation.
        httpd_resp_send_err(request, HTTPD_500_INTERNAL_SERVER_ERROR,
                            "Could not create response");
        return ESP_OK;
    }

    // Send the provisioning result before starting the potentially slow join.
    err = httpd_resp_send(
        request,
        response_body,
        HTTPD_RESP_USE_STRLEN
    );

    if (err == ESP_OK) { // Start connecting only after the HTTP acknowledgement was sent.
        esp_err_t connect_err = esp_wifi_connect();
        if (connect_err != ESP_OK) {
            ESP_LOGW(TAG, "Initial Wi-Fi connect request failed: %s",
                     esp_err_to_name(connect_err));
        } else {
            ESP_LOGI(TAG, "Saved Wi-Fi details; joining configured network");
        }
    }

    return err; // Return the HTTP send result to the server framework.
}

// Start the HTTP server and expose the provisioning endpoint. If registration
// fails, stop the server so no partially initialized server is left running.
esp_err_t start_provisioning_server(void)
{
    httpd_config_t config = HTTPD_DEFAULT_CONFIG(); // Start from ESP-IDF's standard HTTP server settings.
    config.server_port = 80; // Serve provisioning requests on the default HTTP port.

    esp_err_t err = httpd_start(&s_http_server, &config); // Start the server and save its handle.
    if (err != ESP_OK) {
        return err;
    }

    httpd_uri_t provision_uri = { // Map the endpoint path and HTTP method to its handler.
        .uri = "/provision",
        .method = HTTP_POST,
        .handler = provision_handler,
        .user_ctx = NULL,
    };

    err = httpd_register_uri_handler(s_http_server, &provision_uri); // Register POST /provision.
    if (err != ESP_OK) {
        httpd_stop(s_http_server); // Undo server startup if the route could not be registered.
        s_http_server = NULL; // Mark the handle as unavailable after stopping it.
    }

    return err;
}

// React to Wi-Fi/IP events: report AP readiness, retry station connections, and
// signal other tasks once the station has received an IP address.
void wifi_event_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data)
{
    (void)arg;

    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_AP_START) {
        ESP_LOGI(TAG, "Setup Wi-Fi is ready: SSID=%s", SETUP_AP_SSID);
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        wifi_event_sta_disconnected_t *event = (wifi_event_sta_disconnected_t *)event_data;

        ESP_LOGW(TAG, "Wi-Fi disconnected, reason=%d", event->reason);
        xEventGroupClearBits(s_wifi_events, WIFI_CONNECTED_BIT);

        if (atomic_load(&s_has_credentials)) {
            esp_err_t err = esp_wifi_connect();
            if (err != ESP_OK) {
                ESP_LOGW(TAG, "Reconnect request failed: %s",
                         esp_err_to_name(err));
            }
        }
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;

        ESP_LOGI(TAG, "Joined Wi-Fi; IP=" IPSTR " gateway=" IPSTR,
                 IP2STR(&event->ip_info.ip),
                 IP2STR(&event->ip_info.gw));

        esp_err_t mode_err = esp_wifi_set_mode(WIFI_MODE_STA);
        if (mode_err != ESP_OK) {
            ESP_LOGE(TAG, "Could not stop setup AP: %s",
                     esp_err_to_name(mode_err));
        } else {
            ESP_LOGI(TAG, "Setup AP stopped");
        }

        xEventGroupSetBits(s_wifi_events, WIFI_CONNECTED_BIT);
    }
}


// TCP send() may write fewer bytes than requested, so loop until all bytes are
// sent; retry only when the call was interrupted by a signal.
bool send_all(int socket_fd, const char *data, size_t length)
{
    size_t sent = 0; // Number of bytes already accepted by the socket.

    while (sent < length) {
        int result = send(socket_fd, data + sent, length - sent, 0); // Send only the remaining bytes.

        if (result < 0 && errno == EINTR) {
            continue; // A signal interrupted the call; retry without losing progress.
        }
        if (result <= 0) {
            return false; // No progress or a socket error prevents completing the message.
        }

        sent += (size_t)result; // Account for this partial or complete write.
    }

    return true; // Every requested byte was sent.
}

// Receive one newline-terminated TCP control message before a fixed deadline.
// Reading one byte at a time leaves any following message in the socket.
bool receive_line(int socket_fd, char *line, size_t capacity, uint32_t timeout_ms)
{
    size_t used = 0; // Number of non-newline characters stored so far.
    int64_t deadline = esp_timer_get_time() + (int64_t)timeout_ms * 1000; // Set one absolute timeout for the whole line.

    while (used + 1 < capacity) { // Keep one byte free for the terminating NUL.
        int64_t remaining = deadline - esp_timer_get_time(); // Recompute time left after every received byte.
        if (remaining <= 0) return false; // Stop when the total timeout expires.

        fd_set read_set; // File descriptor set passed to select().
        FD_ZERO(&read_set); // Start with an empty set.
        FD_SET(socket_fd, &read_set); // Wait for data on this TCP socket.

        struct timeval timeout = { // Convert the remaining microseconds to select()'s format.
            .tv_sec = remaining / 1000000,
            .tv_usec = remaining % 1000000,
        };

        // select() waits only for the remaining time, so repeated interruptions
        // do not extend the overall timeout.
        int ready = select(socket_fd + 1, &read_set, NULL, NULL, &timeout);

        if (ready < 0 && errno == EINTR) continue; // Retry if a signal interrupted select().
        if (ready <= 0) return false; // Timeout or socket-selection error means no complete line.

        char ch; // Hold the single byte read from the socket.
        int received = recv(socket_fd, &ch, 1, 0); // Read one byte to avoid consuming bytes after the newline.
        if (received <= 0) return false; // The peer closed the connection or the read failed.

        if (ch == '\n') {
            if (used > 0 && line[used - 1] == '\r') used--; // Remove CR from a CRLF line ending.
            line[used] = '\0'; // Terminate the received text as a C string.
            return true; // A full newline-delimited line was received.
        }

        line[used++] = ch; // Append this ordinary character and advance the write position.
    }

    return false; // The line exceeded the output buffer's usable capacity.
}


/*
 * On a phone hotspot, the phone is commonly the ESP32's gateway.
 * This helper supports that connection path when it is enabled by the caller.
 */
uint32_t get_gateway_ip(void)
{
    esp_netif_ip_info_t ip_info; // Structure populated with station IP, netmask, and gateway.

    if (esp_netif_get_ip_info(s_sta_netif, &ip_info) != ESP_OK) { // The interface may not have valid IP details yet.
        return 0; // Use zero to indicate that no gateway address is available.
    }

    return ip_info.gw.addr; // Return the gateway in the network byte order used by lwIP.
}

/*
 * Find the phone's advertised _solarconnect._tcp service, resolve its hostname
 * to IPv4, and return the address. The phone app must publish this service on
 * TCP port 8080.
 */
bool resolve_phone_ip(uint32_t *phone_ip)
{
    // First find a service instance, then resolve its SRV record (host and port),
    // and finally look up that host's IPv4 address.
    mdns_result_t *services = NULL; // Receives the matching mDNS service-instance list.

    esp_err_t err = mdns_query_ptr(
        "_solarconnect",
        "_tcp",
        MDNS_QUERY_TIMEOUT_MS,
        5,
        &services
    );

    if (err != ESP_OK || services == NULL) { // No usable advertised service was found.
        ESP_LOGW(TAG, "mDNS service _solarconnect._tcp not found: %s",
                 esp_err_to_name(err));
        mdns_query_results_free(services);
        return false;
    }

    const char *instance_name = services->instance_name; // The PTR result identifies the service instance.
    if (instance_name == NULL) { // An instance name is needed to query its SRV record.
        ESP_LOGW(TAG, "mDNS service result has no instance name");
        mdns_query_results_free(services);
        return false;
    }

    mdns_result_t *service = NULL; // Receives the service target hostname and TCP port.
    err = mdns_query_srv(
        instance_name,
        "_solarconnect",
        "_tcp",
        MDNS_QUERY_TIMEOUT_MS,
        &service
    );

    mdns_query_results_free(services); // The PTR list is no longer needed after extracting its instance name.

    if (err != ESP_OK || service == NULL || // Require an SRV result with a target and the expected port.
        service->hostname == NULL ||
        service->port != APP_SERVER_PORT) {
        ESP_LOGW(TAG, "Could not resolve service target or port: %s",
                 esp_err_to_name(err));
        mdns_query_results_free(service);
        return false;
    }

    esp_ip4_addr_t address = {0}; // Destination for the resolved IPv4 address.
    err = mdns_query_a(
        service->hostname,
        MDNS_QUERY_TIMEOUT_MS,
        &address
    );

    if (err != ESP_OK || address.addr == 0) { // Reject lookup failure and the unspecified address.
        ESP_LOGW(TAG, "Could not resolve mDNS target %s: %s",
                 service->hostname, esp_err_to_name(err));
        mdns_query_results_free(service);
        return false;
    }

    *phone_ip = address.addr; // Return the resolved IPv4 address to the caller.

    char ip_text[INET_ADDRSTRLEN] = {0}; // Buffer for a human-readable IPv4 address.
    inet_ntop(AF_INET, phone_ip, ip_text, sizeof(ip_text)); // Convert the binary address for logging.

    ESP_LOGI(TAG, "mDNS service resolved to %s:%d",
             ip_text, service->port);

    mdns_query_results_free(service); // Release the SRV result before returning.
    return true; // The output parameter now contains a valid phone address.
}

// Connect to the phone, verify the app/device identity exchange, then relay
// heartbeat and application packets until the TCP connection ends.
bool run_tcp_connection(uint32_t phone_ip)
{
    // Take a local snapshot of the settings used throughout this connection.
    device_config_t config = s_device_config; // Snapshot shared settings so this connection uses one consistent config.
    int socket_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_IP); // Create an IPv4 TCP socket.
    if (socket_fd < 0) {
        ESP_LOGE(TAG, "Could not create TCP socket: errno=%d", errno);
        return false;
    }

    struct sockaddr_in server_address = {
        .sin_family = AF_INET, // Use IPv4 addressing.
        .sin_port = htons(APP_SERVER_PORT), // Convert the port to network byte order.
        .sin_addr.s_addr = phone_ip, // Use the IP discovered through mDNS.
    };

    // Convert the address only for readable diagnostics; connect() uses the
    // binary address stored in server_address.
    char phone[INET_ADDRSTRLEN] = {0};
    inet_ntop(AF_INET, &server_address.sin_addr, phone, sizeof(phone));
    // Log the Phone address and port the ESP32 is about to connect to
    ESP_LOGI(TAG, "Connecting to phone TCP server at %s:%d", phone, APP_SERVER_PORT);

    // Establish the TCP connection to the phone's advertised server.
    if (connect(socket_fd, (struct sockaddr *)&server_address, sizeof(server_address)) != 0) {
        ESP_LOGW(TAG, "TCP connect failed: errno=%d", errno);
        close(socket_fd);
        return false;
    }

    // Identify this device and app to the phone before exchanging packets.
    char greeting[128];
    int greeting_length = snprintf(greeting, sizeof(greeting), "HELLO:%s:%s\n", config.app_id, s_device_id); // Build the line-based identity greeting.
    if (greeting_length <= 0 ||
        (size_t)greeting_length >= sizeof(greeting) ||
        !send_all(socket_fd, greeting, (size_t)greeting_length)) {
        ESP_LOGW(TAG, "Could not send the app ID greeting");
        close(socket_fd);
        return false;
    }

    // Accept the connection only if the phone returns the expected identity.
    char response[128];
    if(!receive_line(socket_fd, response, sizeof(response), 5000)) {
        ESP_LOGW(TAG, "Could not send identity greeting");
        close(socket_fd);
        return false;
    }

    char expected_response[128]; // Buffer for the only authentication response this device accepts.
    int expected_length = snprintf(expected_response,sizeof(expected_response),"AUTH_OK:%s:%s",config.app_id, s_device_id); // Construct the expected response from the same IDs.

    if (expected_length <= 0 ||(size_t)expected_length >= sizeof(expected_response) ||strcmp(response, expected_response) != 0) {
        ESP_LOGW(TAG, "App identity check failed; connection rejected");
        close(socket_fd);
        return false;
    }

    ESP_LOGI(TAG, "App and ESP32 identities verified");

    // TCP is a byte stream, so reads may deliver arbitrary chunks. The parser
    // below inspects the bytes currently accumulated in this buffer.
    char receive_buffer[SERVER_PACKET_MAX_SIZE * 2];
    size_t receive_length = 0;
    // bool request_pending = false;
    // uint32_t pending_request_id = 0;
    // server_packet_t pending_packet = {0};
    // bool awaiting_ack = false;
    // uint8_t retry_count = 0;
    // int64_t ack_deadline = 0;
    bool connected = true; // Controls the socket-processing loop.

    while (connected) {
        // Forward one queued response per loop iteration when one is ready.
        app_response_t outbound_response = {0}; // Clear the response structure before queue receive.
        if (xQueueReceive(s_app_response_queue, &outbound_response, 0) == pdTRUE) { // Check without blocking so socket reads continue.
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
                ESP_LOG_BUFFER_HEX_LEVEL(TAG, outbound_response.packet.data,
                                         outbound_response.packet.length,
                                         ESP_LOG_INFO);
            } else {
                ESP_LOGW(TAG, "Queued response has invalid length: %zu",
                         outbound_response.packet.length);
            }
        }

        fd_set read_set; // Socket set monitored for incoming phone data.
        FD_ZERO(&read_set); // Remove any descriptors from the previous iteration.
        FD_SET(socket_fd, &read_set); // Add the connected phone socket.

        // Poll the socket briefly so the loop can also service queued responses.
        struct timeval timeout = {.tv_sec = 0, .tv_usec = 20000};
        int ready = select(socket_fd + 1, &read_set, NULL, NULL, &timeout); // Wait briefly for incoming data.

        if (ready < 0 && errno != EINTR) {
            ESP_LOGW(TAG, "TCP select failed: errno=%d", errno);
            break;
        }

        if (ready > 0 && FD_ISSET(socket_fd, &read_set)) {
            char incoming[128]; // Temporary chunk; TCP may return fewer bytes than this buffer holds.
            int received = recv(socket_fd, incoming, sizeof(incoming), 0); // Read the available bytes from the phone.

            if (received <= 0) {
                ESP_LOGI(TAG, "Phone closed the TCP connection");
                break;
            }

            if ((size_t)received > sizeof(receive_buffer) - receive_length) { // Ensure the new chunk fits in the accumulator.
                ESP_LOGE(TAG, "TCP receive buffer overflow; closing connection");
                connected = false;
                break;
            }

            memcpy(receive_buffer + receive_length, incoming, (size_t)received); // Append the chunk after existing buffered bytes.
            receive_length += (size_t)received; // Update the count of buffered data.
        }

        while (connected && receive_length > 0) {
            static const char ping[] = "PING\n"; // Expected heartbeat request including its line terminator.

            // Heartbeats are plain newline-terminated control messages and are
            // handled before application packets.
            if (receive_length >= sizeof(ping) - 1 && memcmp(receive_buffer, ping, sizeof(ping) - 1) == 0) {
                static const char pong[] = "PONG\n"; // Matching heartbeat response sent to the phone.
                if (!send_all(socket_fd, pong, sizeof(pong) - 1)) {
                    ESP_LOGW(TAG, "Could not send heartbeat response: PONG");
                    connected = false;
                    break;
                }
                ESP_LOGI(TAG, "Received heartbeat request: PING; sent PONG");
                memmove(receive_buffer, // Shift any bytes after PING to the start of the accumulator.
                        receive_buffer + sizeof(ping) - 1,
                        receive_length - (sizeof(ping) - 1));
                receive_length -= sizeof(ping) - 1; // Remove the consumed heartbeat bytes from the length.
                continue;
            }

            if (receive_length < 2) { // Wait until there are enough bytes to inspect the two-byte prefix.
                break;
            }


            // Application frames start with "##" and end with four ASCII hex
            // digits containing the CRC. Verify the frame before queueing it.
            if (receive_buffer[0] == '#' && receive_buffer[1] == '#') {
                ESP_LOGI(TAG, "receive_buffer raw: len=%zu data=", receive_length);
                if (receive_length < 8) { // Reject a frame too short to contain a prefix, content, and CRC text.
                    ESP_LOGW(TAG, "Discarding short packet: len=%zu", receive_length);
                    memset(receive_buffer, 0, sizeof(receive_buffer)); // Discard all bytes from this malformed frame.
                    receive_length = 0; // Mark the accumulator empty.
                    vTaskDelay(pdMS_TO_TICKS(10)); // Yield briefly before parsing more input.
                    continue; // Restart the parser loop with the now-empty buffer.
                }

                // CRC-16/Modbus: initialize to 0xFFFF and use polynomial 0xA001.
                uint16_t calculated_crc = 0xFFFF; // Initial CRC state specified by the wire protocol.
                for (size_t i = 0; i < receive_length - 4; i++) { // Exclude the four text characters carrying the CRC.
                    calculated_crc ^= (uint8_t)receive_buffer[i]; // Mix the next packet byte into the CRC state.
                    for (uint8_t bit = 0; bit < 8; bit++) { // Apply the CRC polynomial once for each bit.
                        calculated_crc = (calculated_crc & 1U)
                            ? (calculated_crc >> 1) ^ 0xA001U
                            : calculated_crc >> 1;
                    }
                }

                if (receive_length < 4) { // Guard the four-byte CRC extraction below.
                    ESP_LOGW(TAG, "Packet too short for CRC: len=%zu", receive_length);
                    memset(receive_buffer, 0, sizeof(receive_buffer)); // Discard the incomplete packet bytes.
                    receive_length = 0; // Reset the buffered byte count.
                    vTaskDelay(pdMS_TO_TICKS(10)); // Yield to other tasks before continuing.
                    continue; // Start the next parser pass.
                }

                uint16_t received_crc = (uint16_t)strtoul( // Parse the final four ASCII characters as a hexadecimal number.
                    (char[]){ // Build a temporary, NUL-terminated string from the CRC characters.
                        receive_buffer[receive_length - 4], // First CRC digit from the end of the packet.
                        receive_buffer[receive_length - 3], // Second CRC digit.
                        receive_buffer[receive_length - 2], // Third CRC digit.
                        receive_buffer[receive_length - 1], // Fourth CRC digit.
                        '\0' // Terminate the temporary string for strtoul().
                    },
                    NULL, // No end pointer is needed because the packet format fixes the field width.
                    16 // Interpret the characters in base 16.
                );
                if (calculated_crc == received_crc) { // Accept the packet only when its computed and supplied CRCs agree.
                    // Pass a copy to the packet-processing task; it owns the
                    // queued value after this receive buffer is cleared.
                    ESP_LOGI(TAG, "CRC Value: %04X", calculated_crc);
                    server_packet_t packet = {0}; // Create a queue item with unused bytes initialized to zero.
                    packet.length = receive_length; // Preserve the number of valid packet bytes.
                    if (packet.length > sizeof(packet.data)) {
                        ESP_LOGW(TAG, "TCP packet exceeds queue buffer size: %zu", packet.length);
                        memset(receive_buffer, 0, sizeof(receive_buffer));
                        receive_length = 0;
                        continue;
                    }
                    memcpy(packet.data, receive_buffer, packet.length); // Copy the frame into queue-owned storage.
                    if (xQueueSend(s_server_packet_queue, &packet, 0) == pdTRUE) { // Enqueue without blocking the network task.
                        memset(receive_buffer, 0, sizeof(receive_buffer)); // Clear stale packet bytes from the accumulator.
                        receive_length = 0; // Mark the accumulator empty after successful enqueue.
                    } else {
                        ESP_LOGW(TAG, "Queue full; dropped packet");
                        memset(receive_buffer, 0, sizeof(receive_buffer)); // Discard the packet because there is no queue capacity.
                        receive_length = 0; // Do not retry the dropped packet on the next loop.
                    }
                } else {
                    ESP_LOGW(TAG, "CRC mismatch: calc=%04X recv=%04X; dropping packet",
                             calculated_crc, received_crc);
                    memset(receive_buffer, 0, sizeof(receive_buffer)); // Discard bytes that failed integrity validation.
                    receive_length = 0; // Reset the accumulator before receiving more data.
                    vTaskDelay(pdMS_TO_TICKS(10)); // Yield briefly after rejecting the frame.
                }
            } else {
                ESP_LOGW(TAG, "Discarding non-APP packet prefix; len=%zu", receive_length);
                memset(receive_buffer, 0, sizeof(receive_buffer)); // Discard bytes that do not match the expected frame prefix.
                receive_length = 0; // Reset the byte count with the discarded content.
                vTaskDelay(pdMS_TO_TICKS(10)); // Yield briefly before checking for more input.
            }
        }
    }

    shutdown(socket_fd, SHUT_RDWR); // Stop sending and receiving before releasing the socket.
    close(socket_fd); // Release the socket descriptor.
    return true; // The connection loop has ended; the caller will retry later.
}

// Wait for Wi-Fi, discover the phone service, and retry the connection at the
// configured interval until the task is stopped.
void tcp_client_task(void *arg)
{
    (void)arg; // This task receives all required state through shared handles.

    for (;;) {
        // Do not search for or connect to the phone until station Wi-Fi is up.
        xEventGroupWaitBits(
            s_wifi_events,
            WIFI_CONNECTED_BIT,
            pdFALSE,
            pdTRUE,
            portMAX_DELAY
        );

        if (!atomic_load(&s_has_credentials)) { // Do not attempt phone discovery before setup is complete.
            continue; // Return to the wait loop until credentials are available.
        }

        // A phone-hotspot gateway-IP fallback could be attempted here; the
        // active path below instead discovers the phone through mDNS.
        // uint32_t gateway = get_gateway_ip();
        // if (gateway != 0 && run_tcp_connection(gateway)) {
        //     vTaskDelay(pdMS_TO_TICKS(TCP_RETRY_DELAY_MS));
        //     continue;
        // }

        // Resolve on every retry so a changed phone IP is not kept stale.
        uint32_t phone_ip = 0; // Output storage for the address discovered by mDNS.
        if (resolve_phone_ip(&phone_ip)) { // Connect only when a valid phone address was resolved.
            run_tcp_connection(phone_ip); // Remain here until that TCP session ends.
        }
        vTaskDelay(pdMS_TO_TICKS(1));
        // vTaskDelay(pdMS_TO_TICKS(TCP_RETRY_DELAY_MS)); // Back off before another discovery/connection attempt.
    }
}
