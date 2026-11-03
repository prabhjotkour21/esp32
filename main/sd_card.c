#include "stdio.h"
#include "sd_card.h"
#include "eeprom.h"

sdmmc_card_t *sd_card = NULL;

// Helper Function : SD Card Inititlization Routine
esp_err_t init_sd_card(void){
    esp_err_t res;

    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        // Dont format if mount failed, just return error
        .format_if_mount_failed = false,
        // Max number of files can open
        .max_files = 5,
        // Minimum file size 16kb
        .allocation_unit_size = 16 * 1024,
    };

    spi_bus_config_t bus_cfg = {
        .mosi_io_num = PIN_NUM_MOSI,
        .miso_io_num = PIN_NUM_MISO,
        .sclk_io_num = PIN_NUM_CLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 4000,
    };

    // SPI2_HOST 
    res = spi_bus_initialize(SPI2_HOST, &bus_cfg, SPI_DMA_CH_AUTO);
    if(res != ESP_OK){
        ESP_EARLY_LOGI("", "FAILED TO INITIALIZE SPI BUS.");
        return res;
    }

    // Set up slot configuration properties
    sdspi_device_config_t slot_config = SDSPI_DEVICE_CONFIG_DEFAULT();
    slot_config.gpio_cs = PIN_NUM_CS;
    slot_config.host_id = SPI2_HOST;

    sdmmc_host_t host_config = SDSPI_HOST_DEFAULT();
    host_config.slot = SPI2_HOST; // Inform the filesystem to target SPI2
    host_config.max_freq_khz = 4000; // 4MHz

    ESP_EARLY_LOGI("","MOUNTING FILESYSTEM ON SD CARD.");

    res = esp_vfs_fat_sdspi_mount(MOUNT_POINT, &host_config, &slot_config, &mount_config, &sd_card);
    if(res != ESP_OK){
        ESP_EARLY_LOGI("", "FAILED TO MOUNT FATFS.");
        return res;
    }

    ESP_EARLY_LOGI("", "SD CARD MOUNTED.");
    return ESP_OK;
}

// Helper Function: File Read Routine
esp_err_t read_file(const char *file_Path){
    FILE *f = fopen(file_Path, "rb");
    if(f == NULL){
        ESP_EARLY_LOGI("", "FAILED TO OPEN FILE.");
        return ESP_FAIL;
    }
    
    fseek(f, 0, SEEK_END);      // Go to end of file
    file_size = ftell(f);  // Get the file size
    fseek(f, 0, SEEK_SET);      // Go to beginning

    // Check the file size
    if(file_size <= 0 || file_size > BUFF_SIZE){
        ESP_EARLY_LOGI("", "INVALID FILE SIZE.");
        fclose(f);
        return ESP_FAIL;
    }

    // Read the whole file
    bytes_read = fread(write_buffer, 1, file_size, f);
    fclose(f);
    return ESP_OK;
}

// Read the Config File Version from the SD card
esp_err_t read_config_version(const char *file_Path){
    FILE *f = fopen(file_Path, "rb");
    if(f == NULL){
        ESP_EARLY_LOGI("", "FAILED TO OPEN FILE.");
        return ESP_FAIL;
    }
    fseek(f, 0, SEEK_SET);      // Go to beginning
    // Read the Config File Version
    size_t bytes_read = fread(Config_Version_File, 1, CONFIG_VER_SIZE, f);
    Config_Version_File[CONFIG_VER_SIZE] = '\0';
    fclose(f);
    if (bytes_read != CONFIG_VER_SIZE) {
        return ESP_FAIL;
    }
    return ESP_OK;  
}

bool parse_config_version(const uint8_t *version, uint32_t *version_number)
{
    static const char prefix[] = "CONFIG_V";
    const uint8_t *value = version;
    uint32_t parsed_version = 0;

    if (strncmp((const char *)value, prefix, sizeof(prefix) - 1) != 0) {
        return false;
    }
    value += sizeof(prefix) - 1;

    if (*value < '0' || *value > '9') {
        return false;
    }

    while (*value >= '0' && *value <= '9') {
        uint8_t digit = *value - '0';
        if (parsed_version > (UINT32_MAX - digit) / 10) {
            return false;
        }
        parsed_version = parsed_version * 10 + digit;
        value++;
    }

    while (*value == ' ' || *value == '\t' || *value == '\r' || *value == '\n') {
        value++;
    }
    if (*value != '\0') {
        return false;
    }

    *version_number = parsed_version;
    return true;
}

bool config_version_is_erased(const uint8_t *version)
{
    for (size_t i = 0; i < CONFIG_VER_SIZE; i++) {
        if (version[i] != 0xFF) {
            return false;
        }
    }
    return true;
}