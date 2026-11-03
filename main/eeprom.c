#include "eeprom.h"

struct D16x2_t Display_16x2[MAX_PAGE_STATE_16x2] = {0};
struct D20x4_t Display_20x4[MAX_PAGE_STATE_20x4] = {0};
uint8_t LCD_MESSAGE[MAX_LCD_MESSAGES_12][12] = {0};
uint8_t LCD_MESSAGE_16[MAX_LCD_MESSAGES_16][16] = {0};
uint8_t LCD_MESSAGE_20[MAX_PAGE_20x4][20] = {0};

uint16_t file_size = 0;
uint16_t bytes_read = 0;
uint8_t write_buffer[BUFF_SIZE] = {0};
uint8_t read_buffer[BUFF_SIZE] = {0};
uint8_t Display[DISPLAY_TYPE + 1] = {0};
uint8_t Config_Version_Partition[CONFIG_VER_SIZE + 1] = {0};
uint8_t Config_Version_File[CONFIG_VER_SIZE + 1] = {0};

bool Write_success = false;
bool Read_success = false;