
#ifndef MAIN_RTC_H
#define MAIN_RTC_H

#include "esp_err.h"

void init_rtc(void);
void setTimeZone(void);
esp_err_t sync_time_over_internet(void);
esp_err_t set_system_time_from_timestamp(uint32_t timestamp);
bool is_valid_unix_timestamp(uint32_t timestamp);

#endif // MAIN_RTC_H