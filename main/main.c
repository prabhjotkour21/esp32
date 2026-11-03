
#include "com.h"
#include "eeprom.h"
#include "sd_card.h"
#include <time.h>
#include <sys/time.h>
#include "wifi.h"
#include "rtc.h"
#include "uart.h"
#include "app.h"



void time_sync_task(void *arg)
{
    (void)arg;

    xEventGroupWaitBits(
        s_wifi_events,
        WIFI_CONNECTED_BIT,
        pdFALSE,
        pdTRUE,
        portMAX_DELAY
    );

    esp_err_t err = sync_time_over_internet();
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Time sync failed: %s", esp_err_to_name(err));
    } else {
        time_t now;
        struct tm timeinfo;
        char time_string[32];

        time(&now);
        if (localtime_r(&now, &timeinfo) != NULL &&
            strftime(time_string, sizeof(time_string),
                     "%Y-%m-%d %H:%M:%S %Z", &timeinfo) > 0) {
            printf("Synchronized local time: %s\n", time_string);
        } else {
            ESP_LOGW(TAG, "Time synchronized, but formatting the local time failed");
        }
    }

    // This is a one-shot task. Free its stack and task control block when done;
    // returning from a FreeRTOS task entry point triggers an abort.
    vTaskDelete(NULL);
}


void app_main(void)
{
    //===================[Initialization]==================
    printf("\n");
    printf("=================================\n");
    printf(" ESP32-C3 Config Flash Test \n");
    printf("==================================\n");

    //===================[Find Custom Partition]============
    esp_err_t err = ESP_OK;
    const esp_partition_t *partition = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, 0x40, CONFIG_PARTITION_NAME);
    if(partition == NULL){
        printf("ERROR : config partition not found \n");
        return;
    }

    printf("Cofig partition found \n");
    printf("Address : 0x%08lx\n", partition->address);
    printf("Size : %lu bytes\n", partition->size);

    // Read the config file version from the partition
    err = esp_partition_read(partition, 0, Config_Version_Partition, CONFIG_VER_SIZE);
    if(err != ESP_OK){
        printf("Failed to read partition config version : %s\n", esp_err_to_name(err));
        return;
    }
    Config_Version_Partition[CONFIG_VER_SIZE] = '\0';
    printf("Config Version (Partition): %s\n", Config_Version_Partition);

    // Read the partition data and Display that data
    err = esp_partition_read(partition, 0, read_buffer, sizeof(read_buffer));
    if (err != ESP_OK){
        printf("Read failed: %s\n",esp_err_to_name(err));
        return;
    }
    printf("Read data:\n");
    for (int i = 0; i < sizeof(read_buffer); i++){
        printf("%02X ",read_buffer[i]);
    }
    printf("\n");

    //==================[Initialize SD Card]================================
    if(init_sd_card() == ESP_OK){
        // Read the Current Config File Version from the SD card
        if (read_config_version("/sdcard/display_config.bin") != ESP_OK) {
            printf("Failed to read config version from file\n");
            Write_success = false;
            return;
        }
        printf("Config Version (File): %s\n", Config_Version_File);

        //Check the Config File Version with the Partition Config Version
        uint32_t file_version;
        uint32_t partition_version;
        if (!parse_config_version(Config_Version_File, &file_version)) {
            printf("Invalid config version in file. Update skipped\n");
            Write_success = false;
            Read_success = true;
        }
        else if (config_version_is_erased(Config_Version_Partition)) {
            printf("No config version stored in partition (0xFF). Update Required\n");
            Write_success = true;
        }
        else if (!parse_config_version(Config_Version_Partition, &partition_version)) {
            printf("Invalid config version in partition. Update skipped\n");
            Write_success = false;
            Read_success = true;
        }
        else if (file_version == partition_version) {
            printf("Config Version Match (V%lu): No Update Required\n",
                   (unsigned long)file_version);
            Write_success = false;
            Read_success = true;
        }
        else if (file_version > partition_version) {
            printf("Newer config version found (File: V%lu, Partition: V%lu): Update Required\n",
                   (unsigned long)file_version, (unsigned long)partition_version);
            Write_success = true;
        }
        else {
            printf("Config downgrade rejected (File: V%lu, Partition: V%lu). Update skipped\n",
                   (unsigned long)file_version, (unsigned long)partition_version);
            Write_success = false;
            Read_success = true;
        }
        // Read the display_config.bin file from the SD card
        printf("SD Card initialization Successful\n");
        if(Write_success){
            if(read_file("/sdcard/display_config.bin") == ESP_OK){
                err = esp_partition_write(partition, 0, write_buffer, bytes_read);
                if (err != ESP_OK){ 
                    printf("Write failed: %s\n",esp_err_to_name(err));
                }
                else{
                    printf("Config written Successfully\n");
                }
                Write_success = false;
                Read_success = true;
            }

        }
        else{
            printf("No Update Required: Skipping display_config.bin Read\n");
        }
    }
    else{
        printf("SD Card initialization Failed\n");
        Write_success = false;
        Read_success = true;
    }

    // Read the partition data and display it
    if(Read_success){
        err = esp_partition_read(partition, 0, read_buffer, sizeof(read_buffer));
        if (err != ESP_OK){
            printf("Read failed: %s\n",esp_err_to_name(err));
            return;
        }
        printf("Read data:\n");
        for (int i = 0; i < sizeof(read_buffer); i++){
            printf("%02X ",read_buffer[i]);
        }
        printf("\n");
        Write_success = false;
        Read_success = false;
    }
    memcpy(Config_Version_Partition, read_buffer, CONFIG_VER_SIZE);
    Config_Version_Partition[CONFIG_VER_SIZE] = '\0';
    printf("Config Version (Partition): %s\n", Config_Version_Partition);
    memcpy(Display, &read_buffer[CONFIG_VER_SIZE], DISPLAY_TYPE);
    Display[DISPLAY_TYPE] = '\0';
    printf("Display Config: %s\n", Display);
    memcpy(Display_16x2, &read_buffer[CONFIG_VER_SIZE + DISPLAY_TYPE], sizeof(Display_16x2));
    // printf("Display_16x2 PageCount: %d\n", Display_16x2[0].PageCount);
    // printf("Display_16x2 PageDelay: %d\n", Display_16x2[0].PageDelay);
    // printf("Page 0 Line 1: %d Line 2: %d\n", Display_16x2[0].Line[0].Line1, Display_16x2[0].Line[0].Line2);
    for(int x = 0; x < MAX_PAGE_STATE_16x2; x++){
        for(int i = 0; i < MAX_PAGE_PER_16x2; i++){
            printf("Page %d Line1: %d Line2: %d\n", i, Display_16x2[x].Line[i].Line1, Display_16x2[x].Line[i].Line2);
        }   
        printf("Display_16x2[%d] PageCount: %d\n", x, Display_16x2[x].PageCount);
        printf("Display_16x2[%d] PageDelay: %d\n", x, Display_16x2[x].PageDelay);
        printf("\n=================================================\n");
    }
    memcpy(LCD_MESSAGE, &read_buffer[CONFIG_VER_SIZE + DISPLAY_TYPE + sizeof(Display_16x2)], sizeof(LCD_MESSAGE));
    for (int i = 0; i < MAX_LCD_MESSAGES_12; i++) {
        printf("LCD_MESSAGE[%d]: %.*s\n",
            i,
            (int)sizeof(LCD_MESSAGE[i]),
            (const char *)LCD_MESSAGE[i]);
    }
    memcpy(LCD_MESSAGE_16, &read_buffer[CONFIG_VER_SIZE + DISPLAY_TYPE + sizeof(Display_16x2) + sizeof(LCD_MESSAGE)], sizeof(LCD_MESSAGE_16));
    for (int i = 0; i < MAX_LCD_MESSAGES_16; i++) {
        printf("LCD_MESSAGE_16[%d]: %.*s\n",
            i,
            (int)sizeof(LCD_MESSAGE_16[i]),
            (const char *)LCD_MESSAGE_16[i]);
    }

    memcpy(Display_20x4, &read_buffer[CONFIG_VER_SIZE + DISPLAY_TYPE + sizeof(Display_16x2) + sizeof(LCD_MESSAGE) + sizeof(LCD_MESSAGE_16)], sizeof(Display_20x4));
    for(int x = 0; x < MAX_PAGE_STATE_20x4; x++){
        for(int i = 0; i < MAX_PAGE_PER_20x4; i++){
            printf("Page %d Line1: %d Line2: %d Line3: %d Line4: %d\n", i, Display_20x4[x].Line[i].Line1, Display_20x4[x].Line[i].Line2, Display_20x4[x].Line[i].Line3, Display_20x4[x].Line[i].Line4);
        }
        printf("Display_20x4[%d] PageCount: %d\n", x, Display_20x4[x].PageCount);
        printf("Display_20x4[%d] PageDelay: %d\n", x, Display_20x4[x].PageDelay);
        printf("\n=================================================\n");
    }

    memcpy(LCD_MESSAGE_20, &read_buffer[CONFIG_VER_SIZE + DISPLAY_TYPE + sizeof(Display_16x2) + sizeof(LCD_MESSAGE) + sizeof(LCD_MESSAGE_16) + sizeof(Display_20x4)], sizeof(LCD_MESSAGE_20));
    for (int i = 0; i < MAX_PAGE_20x4; i++) {
        printf("LCD_MESSAGE_20[%d]: %.*s\n",
            i,
            (int)sizeof(LCD_MESSAGE_20[i]),
            (const char *)LCD_MESSAGE_20[i]);
    }


    //================================[Inititalize NVS]==============================
    err = nvs_flash_init();   // Initialize NVS

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
        APP_COMMAND_QUEUE_LENGTH, sizeof(app_response_t)
    );
    if (s_app_response_queue == NULL) {
        ESP_LOGE(TAG, "Could not create app response queue");
        return;
    }

    //===================[RTC Value Initialize]==============
    init_rtc();

    start_wifi();
    ESP_ERROR_CHECK(start_provisioning_server());

    setTimeZone();

    init_uart_gdma();

    esp_intr_dump(stdout);

    if (xTaskCreate(time_sync_task, "time_sync", 4096, NULL, 5, NULL) != pdPASS) {
        ESP_LOGE(TAG, "Could not start time sync task");
    }

    if (xTaskCreate(app_command_processor_task, "app_command",
                    4096, NULL, 5, NULL) != pdPASS) {
        ESP_LOGE(TAG, "Could not start app command processor task");
        return;
    }

    if (xTaskCreate(tcp_client_task, "tcp_client",
                    4096, NULL, 5, NULL) != pdPASS) {
        ESP_LOGE(TAG, "Could not start TCP client task");
    }

    // if (xTaskCreate(Process_data, "process_data",
    //                 8192, NULL, 5, NULL) != pdPASS) {
    //     ESP_LOGE(TAG, "Could not start packet processing task");
    // }
}
