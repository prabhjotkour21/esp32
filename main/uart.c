#include "uart.h"
#include "com.h"
#include "rtc.h"

static const char *UART_TAG = "uart_gdma";
uhci_controller_handle_t uhci_ctrl;
SemaphoreHandle_t xBufferSem = NULL;
DMA_ATTR uint8_t rx_buffer_a[BUF_SIZE];
DMA_ATTR uint8_t rx_buffer_b[BUF_SIZE];
volatile int active_buffer_idx = 0;
uint32_t last_received_size = 0;

//==============[UART GDMA INTERRUPT & TASK]==============
bool IRAM_ATTR rx_complete_callback(uhci_controller_handle_t handle, const uhci_rx_event_data_t *event, void *user_ctx){
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    if(event->flags.totally_received){
        last_received_size = event->recv_size;
        active_buffer_idx = (active_buffer_idx == 0) ? 1 : 0;
        ESP_EARLY_LOGI("ISR", "size=%lu, total=%d", event->recv_size, event->flags.totally_received);
        if(xBufferSem != NULL){
            xSemaphoreGiveFromISR(xBufferSem, &xHigherPriorityTaskWoken);
        }
    }
    return (xHigherPriorityTaskWoken == pdTRUE);
}

volatile uint8_t packet_count = 0;
void vUARTDataProcessingTask(void *pvParameters){

    static uint8_t accumulated_packet[BUF_SIZE];
    static size_t accumulated_length = 0;
    static int64_t start = 0;
    static int64_t elapsed_us = 0;
    static size_t packet_length = 0;
    static u16 CRC = 0;
    while (1){
        if(xSemaphoreTake(xBufferSem, portMAX_DELAY) == pdTRUE){
            int64_t start = esp_timer_get_time();
            
            uint8_t *p_filled_buffer = (active_buffer_idx == 0) ? &rx_buffer_b[0] : &rx_buffer_a[0];
            uint8_t *p_next_buffer   = (active_buffer_idx == 0) ? &rx_buffer_a[0] : &rx_buffer_b[0];

            memset(p_next_buffer, 0x00, BUF_SIZE);
            packet_count++;
            esp_err_t err = uhci_receive(uhci_ctrl, p_next_buffer, BUF_SIZE);
            printf("\n Packet Count : %d\n", packet_count);
            if (err != ESP_OK) {
                ESP_LOGE(UART_TAG, "Failed to re-arm UHCI reception: %s", esp_err_to_name(err));
            }

            if (last_received_size > 0) {
                const size_t received_size = (last_received_size < BUF_SIZE)
                    ? (size_t)last_received_size : BUF_SIZE;
                ESP_LOGI(UART_TAG, "Successfully captured block of size: %u bytes",
                         (unsigned)received_size);
                // for (size_t i = 0; i < received_size; i++) {
                //     printf("%02X ", p_filled_buffer[i]);
                // }
                // printf("\n");
                last_received_size = 0;

                if (accumulated_length + received_size <= sizeof(accumulated_packet)) {
                    memcpy(accumulated_packet + accumulated_length,
                           p_filled_buffer, received_size);
                    accumulated_length += received_size;
                } else {
                    ESP_LOGW(UART_TAG, "UART packet accumulator overflow; resetting");
                    accumulated_length = 0;
                    memset(p_filled_buffer, 0xFF, BUF_SIZE);
                    vTaskDelay(pdMS_TO_TICKS(1)); 
                    continue;
                }
                memset(p_filled_buffer, 0xFF, BUF_SIZE);

                if (accumulated_length < 6) {
                    vTaskDelay(pdMS_TO_TICKS(1)); 
                    continue;
                }

                packet_length = ((size_t)accumulated_packet[5] >> 1) + 9;
                if (packet_length > BUF_SIZE) {
                    printf("Generated packet length %u exceeds transmit buffer\n",
                           (unsigned)packet_length);
                    accumulated_length = 0;
                    vTaskDelay(pdMS_TO_TICKS(1)); 
                    continue;
                }
                if (packet_length > accumulated_length) {
                    printf("UART packet incomplete: expected %u bytes, accumulated %u\n",
                           (unsigned)packet_length, (unsigned)accumulated_length);
                    vTaskDelay(pdMS_TO_TICKS(1)); 
                    continue;
                }

                for (size_t i = 0; i < packet_length; i++) {
                    Receiver1(&sIOT_COMM, accumulated_packet[i]);
                    // printf("Index [%02lu] : 0x%02X\n", (unsigned long)i, DG_TXB[i]);
                    // printf("Index [%02lu] : 0x%02X\n", (unsigned long)i, sIOT_COMM.TX_DATA[i]);
                    // printf("ID Location [%02X]\n", sIOT_COMM.TX_DATA_INDEX_POSITIONS[i]);
                }

                if (accumulated_length > packet_length) {
                    memmove(accumulated_packet, accumulated_packet + packet_length,
                            accumulated_length - packet_length);
                }
                accumulated_length -= packet_length;

                if (!sIOT_COMM.RX_DATA_RECEIVED || sIOT_COMM.RX_COMM_ERR) {
                    ESP_LOGE(UART_TAG, "UART packet validation failed");
                    sIOT_COMM.RX_DATA_RECEIVED = false;
                    sIOT_COMM.RX_COMM_ERR = false;
                    vTaskDelay(pdMS_TO_TICKS(1)); 
                    continue;
                }

                packet_length = sIOT_COMM.RX_DATA_MAX;
                if (packet_length > sizeof(sIOT_COMM.RX_DATA) ||
                    sIOT_COMM.RX_DATA_INDEX_COUNTS > MAX_IDinPACKET) {
                    ESP_LOGE(UART_TAG, "Decoded UART packet exceeds limits");
                    sIOT_COMM.RX_DATA_RECEIVED = false;
                    sIOT_COMM.RX_COMM_ERR = false;
                    vTaskDelay(pdMS_TO_TICKS(1)); 
                    continue;
                }

                total_length = (packet_length * 2) + P_HEADER_SIZE + sIOT_COMM.RX_DATA_INDEX_COUNTS;
                if (total_length > sizeof(ACT) || total_length + 6 > sizeof(TXB)) {
                    ESP_LOGE(UART_TAG, "Encoded UART response exceeds limits: %u", (unsigned)total_length);
                    sIOT_COMM.RX_DATA_RECEIVED = false;
                    sIOT_COMM.RX_COMM_ERR = false;
                    vTaskDelay(pdMS_TO_TICKS(1)); 
                    continue;
                }
                // else{
                //     printf("Total Length Of Packet: %d\n", total_length);
                // }
                if(sIOT_COMM.RX_DATA_RECEIVED){
                printf("--- Data Receiving Success ---\n");
                }
                else{
                    printf("--- Data Receiving Failed ---\n");
                }
                sIOT_COMM.RX_DATA_RECEIVED = false;
                sIOT_COMM.RX_COMM_ERR = false;
            }
            app_command_t command = {0};

            if (xQueueReceive(s_app_command_queue, &command, 0) == pdTRUE) {
                // printf("Received app command: type=%d request_id=%lu table=%s version=%s\n",
                //        command.type,
                //        (unsigned long)command.request_id,
                //        command.table_number,
                //        command.version);
                if(command.packet_type == Display_Packet_TYPE){
                    memset(ACT, 0x00, 256);
                    // Start Characters (2 bytes)
                    ACT[0] = APP_START_CHARACTER; 
                    ACT[1] = APP_START_CHARACTER;
                    // Product Version (8 bytes)
                    for(size_t i = 0, j = 2; i < PRODUCT_VERSION_SIZE; i++, j+=2){
                        putByte(&ACT[j], (uint8_t)PRODUCT_VERSION[i]);
                    }
                    putByte(&ACT[18], (uint8_t)ESP32); // Destination
                    putByte(&ACT[20], (uint8_t)APP);   // Source
                    putByte(&ACT[22], (uint8_t)Display_Packet_TYPE); // Packet Type
                    putByte(&ACT[24], (uint8_t)Display_Packet_TYPE); // Table Number
                    putByte(&ACT[26], (uint8_t)((packet_length*2)+sIOT_COMM.RX_DATA_INDEX_COUNTS+2)); // Packet Size (CRC Not Included)
                    bool layout_valid = true;
                    for(size_t i = P_HEADER_SIZE, j = 0, k = 0, cnt = 0; i < total_length; ){
                        if(k < sIOT_COMM.RX_DATA_INDEX_COUNTS &&
                        i == (sIOT_COMM.RX_DATA_INDEX_POSITIONS[k] + P_HEADER_SIZE + cnt + j)){
                            ACT[i] = '$';
                            cnt++;
                            k++;
                            i++;
                        }
                        else{
                            if (j >= packet_length || i + 1 >= total_length) {
                                printf("Invalid encoded packet layout\n");
                                layout_valid = false;
                                break;
                            }
                            putByte(&ACT[i], sIOT_COMM.RX_DATA[j]);
                            i+=2;
                            j++;
                        }
                    }
                    if (!layout_valid) {
                        sIOT_COMM.RX_DATA_RECEIVED = false;
                        sIOT_COMM.RX_COMM_ERR = false;
                        vTaskDelay(pdMS_TO_TICKS(1)); 
                        continue;
                    }
                    putByte(&ACT[total_length], (uint8_t)(VALID_PACKET)); // Valid Packet Flag

                    CRC = 0xFFFF; // CRC Initialization
                    for (size_t i = 0; i < ((packet_length * 2) + P_HEADER_SIZE + sIOT_COMM.RX_DATA_INDEX_COUNTS + 2); i++) {
                        // printf("0x%02X\n",ACT[i]);
                        CRC = FIND_CRC(CRC, ACT[i]);
                    }
                    memset(TXB, 0x00, 256);
                    memcpy(TXB, ACT, total_length+2);
                    putByte(&TXB[(total_length)+2], (uint8_t)(CRC >> 8) & 0xFF);
                    putByte(&TXB[(total_length)+4], (uint8_t)(CRC >> 0) & 0xFF);
                    printf("CRC : [0x%02X], Length = %d\n", CRC, total_length);

                    server_packet_t send_packet = {0};
                    send_packet.length = (total_length + 6);
                    if (send_packet.length > sizeof(send_packet.data)) {
                        printf("Response packet too large: %u bytes\n", (unsigned)send_packet.length);
                        vTaskDelay(pdMS_TO_TICKS(1)); 
                        continue;
                    }
                    memcpy(send_packet.data, TXB, send_packet.length);
                    printf("TXB response hex: ");
                    for (size_t i = 0; i < send_packet.length && i < 32; i++) {
                        printf("%02X ", send_packet.data[i]);
                    }
                    printf("\n");

                    app_response_t response = {0};
                    response.request_id = command.request_id;
                    response.success = true;
                    response.packet = send_packet;
                    printf("Inside the COMMAND Queue\n");
                    if (xQueueSend(s_app_response_queue, &response, 0) != pdTRUE) {
                        printf("Failed to queue outbound server packet\n");
                    } else {
                        printf("Queued outbound server packet of length %u\n", (unsigned)response.packet.length);
                    }
                    // printf("Server Packet\n");
                    // for (size_t i = 0; i < ((total_length)+6); i++) {
                    // 	printf("0x%02X,\n",TXB[i]);
                    // }
                    elapsed_us = esp_timer_get_time() - start;
                    ESP_LOGI("", "Function took %lld us", (long long)elapsed_us);

                }
                else if(command.packet_type == Timestamp_Packet_TYPE){
                    esp_err_t result = set_system_time_from_timestamp(command.timestamp);
                    CRC = 0xFFFF; // CRC Initialization
                    packet_length = 0; // No additional data for timestamp response
                    memset(ACT, 0x00, sizeof(ACT));
                    memset(TXB, 0x00, sizeof(TXB));
                    // Start Characters (2 bytes)
                    ACT[0] = APP_START_CHARACTER; 
                    ACT[1] = APP_START_CHARACTER;
                    // Product Version (8 bytes)
                    for(size_t i = 0, j = 2; i < PRODUCT_VERSION_SIZE; i++, j+=2){
                        putByte(&ACT[j], (uint8_t)PRODUCT_VERSION[i]);
                    }

                    putByte(&ACT[18], (uint8_t)APP); // Destination
                    putByte(&ACT[20], (uint8_t)ESP32);   // Source
                    putByte(&ACT[22], (uint8_t)Timestamp_Packet_TYPE); // Packet Type
                    putByte(&ACT[24], (uint8_t)0); // Table Number
                    putByte(&ACT[26], (uint8_t)((packet_length*2)+P_HEADER_SIZE+2)); // Length
                    
                    if(result == ESP_OK){
                        putByte(&ACT[28], (uint8_t)VALID_PACKET); // Success
                        printf("Timestamp Set Successfully \n");
                    } else {
                        putByte(&ACT[28], (uint8_t)INVALID_PACKET); // Failure
                        printf("Timestamp Set Failed \n");
                    }
                    const size_t timestamp_status_length = (packet_length * 2) + P_HEADER_SIZE + 2;
                    for (size_t i = 0; i < timestamp_status_length; i++) {
                        CRC = FIND_CRC(CRC, ACT[i]);
                    }
                    memset(TXB, 0x00, sizeof(TXB));
                    memcpy(TXB, ACT, timestamp_status_length);
                    putByte(&TXB[timestamp_status_length], (uint8_t)((CRC >> 8) & 0xFF));
                    putByte(&TXB[timestamp_status_length + 2], (uint8_t)(CRC & 0xFF));
                    server_packet_t time_packet = {0};
                    time_packet.length = timestamp_status_length + 4;
                    memcpy(time_packet.data, TXB, time_packet.length);
                    app_response_t response = {0};
                    response.request_id = command.request_id;
                    response.success = (result == ESP_OK);
                    response.packet = time_packet;
                    if (xQueueSend(s_app_response_queue, &response, 0) == pdTRUE) {
                        printf("Timestamp Response Queued Successfully\n");
                    } else {
                        printf("Failed to queue timestamp response packet\n");
                    }
                }
		    }
            else{
                elapsed_us = esp_timer_get_time() - start;
                ESP_LOGI("", "Function took %lld us", (long long)elapsed_us);
                printf("No command in the queue\n");
            }
        }
        vTaskDelay(pdMS_TO_TICKS(1)); 
    }    
}

void init_uart_gdma(void){
    // Initialize UART with GDMA
    uart_config_t uart_cfg = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    ESP_ERROR_CHECK(uart_param_config(UART_PORT, &uart_cfg));
    ESP_ERROR_CHECK(uart_set_rx_timeout(UART_PORT, 20));
    ESP_ERROR_CHECK(uart_set_pin(UART_PORT, TX_PIN, RX_PIN, -1, -1));

    // Create Synchronization Primitives for UART Pipeline
    xBufferSem = xSemaphoreCreateBinary();
    if (xBufferSem == NULL) {
        ESP_LOGE(UART_TAG, "Failed to create binary semaphore.");
        return;
    }
    // UHCI Controller Config & Initialization
    uhci_controller_config_t uhci_cfg = {
        .uart_port = UART_PORT,                 
        .tx_trans_queue_depth = 30,             
        .max_transmit_size = BUF_SIZE,          
        .max_receive_internal_mem = BUF_SIZE,   
        .dma_burst_size = 4,
        .rx_eof_flags.idle_eof = 1,        
        .rx_eof_flags.length_eof = 1,           
    };

    ESP_ERROR_CHECK(uhci_new_controller(&uhci_cfg, &uhci_ctrl));

    uhci_event_callbacks_t cbs = {
        .on_rx_trans_event = rx_complete_callback,
    };
    ESP_ERROR_CHECK(uhci_register_event_callbacks(uhci_ctrl, &cbs, NULL));

    xTaskCreate(vUARTDataProcessingTask, "UART_Processor", 2048, NULL, 6, NULL);
    ESP_LOGI(UART_TAG, "UART GDMA Pipeline Configured Successfully.");

    // Fire initial hardware reception using first ping buffer
    ESP_ERROR_CHECK(uhci_receive(uhci_ctrl, &rx_buffer_a[0], BUF_SIZE));
    
}