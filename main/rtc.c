#include "rtc.h"
#include "esp_log.h"
#include "esp_sntp.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <sys/time.h>

static const char *TAG = "rtc";

//=================[Variable Declaration]==================
time_t now;
struct tm timeinfo;


void init_rtc(void){
    time(&now);
    localtime_r(&now, &timeinfo);
}

//================[SNTP Sync Attempt]============
esp_err_t sync_time_over_internet(void){
    uint32_t wait_intervals = 0;

    printf("Inititlizing SNTP client...");
    esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
    esp_sntp_setservername(0, "pool.ntp.org");
    esp_sntp_setservername(1, "time.google.com");
    esp_sntp_init();

    while (sntp_get_sync_status() != SNTP_SYNC_STATUS_COMPLETED) {
        vTaskDelay(pdMS_TO_TICKS(2000));
        if (sntp_get_sync_status() != SNTP_SYNC_STATUS_COMPLETED &&
            ++wait_intervals % 15 == 0) {
            ESP_LOGI(TAG, "Still waiting for internet NTP response...");
        }
    }

    ESP_LOGI(TAG, "Time synchronized via internet NTP; SNTP remains active");
    return ESP_OK;
}

void setTimeZone(void){
    setenv("TZ", "IST-5:30", 1);
    tzset();
}

//================[Set Time]============
bool is_valid_unix_timestamp(uint32_t timestamp){
    // Accept timestamps from 2020-01-01 through 2099-12-31 
    return timestamp >= 1577836800U && timestamp <= 4102444799U;
}

esp_err_t set_system_time_from_timestamp(uint32_t timestamp){
    
    if (!is_valid_unix_timestamp(timestamp)) {
        ESP_LOGW(TAG, "Invalid Unix timestamp: %lu",
                 (unsigned long)timestamp);
        return ESP_ERR_INVALID_ARG;
    }
    struct timeval current_time = {
        .tv_sec = (time_t)timestamp,
        .tv_usec = 0
    };
    if (settimeofday(&current_time, NULL) != 0) {
        ESP_LOGE(TAG, "Failed to set system RTC");
        return ESP_FAIL;
    }
    time(&now);
    if (localtime_r(&now, &timeinfo) != NULL) {
        char date_time[32];

        if (strftime(date_time, sizeof(date_time),
                     "%Y-%m-%d %H:%M:%S %Z",
                     &timeinfo) > 0) {
            ESP_LOGI(TAG, "RTC set to: %s", date_time);
        }
    }
    return ESP_OK;
}
