#ifndef MAIN_SD_CARD_H
#define MAIN_SD_CARD_H

#include "stdint.h"
#include "esp_err.h"
#include "sdmmc_cmd.h"
#include "esp_vfs_fat.h"
#include "driver/spi_master.h"
#include "driver/sdspi_host.h"

#define PIN_NUM_MOSI    7
#define PIN_NUM_MISO    2
#define PIN_NUM_CLK     6
#define PIN_NUM_CS      10


#define MOUNT_POINT "/sdcard"
extern sdmmc_card_t *sd_card;


esp_err_t init_sd_card(void);
esp_err_t read_file(const char *file_Path);
esp_err_t read_config_version(const char *file_Path);
bool config_version_is_erased(const uint8_t *version);
bool parse_config_version(const uint8_t *version, uint32_t *version_number);


#endif // MAIN_SD_CARD_H