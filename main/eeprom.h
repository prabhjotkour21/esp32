
#ifndef MAIN_EEPROM_H
#define MAIN_EEPROM_H


#include <stdint.h>

//===================[Defines]=======================
#define CONFIG_PARTITION_NAME "config"

#define BUFF_SIZE             4096
#define DISPLAY_TYPE          13

#define MAX_PAGE_PER_16x2     12
#define MAX_PAGE_STATE_16x2   8

#define MAX_LCD_MESSAGES_12   52
#define MAX_LCD_MESSAGES_16   24

#define MAX_PAGE_PER_20x4     6
#define MAX_PAGE_STATE_20x4   8

#define MAX_PAGE_20x4         10
#define CONFIG_VER_SIZE       10

//=====================[Structure Declaration]=============
struct line_t{
    uint8_t Line1;
    uint8_t Line2;
};

struct D16x2_t{
    struct line_t Line[MAX_PAGE_PER_16x2];
    uint8_t PageCount;
    uint8_t PageDelay;
};

struct line4_t{
    uint8_t Line1;
    uint8_t Line2;
    uint8_t Line3;
    uint8_t Line4;
};

struct D20x4_t{
    struct line4_t Line[MAX_PAGE_PER_20x4];
    uint8_t PageCount;
    uint8_t PageDelay;
};

//=================[Extern Variables]=========
extern struct D16x2_t Display_16x2[MAX_PAGE_STATE_16x2];
extern struct D20x4_t Display_20x4[MAX_PAGE_STATE_20x4];
extern uint8_t LCD_MESSAGE[MAX_LCD_MESSAGES_12][12];
extern uint8_t LCD_MESSAGE_16[MAX_LCD_MESSAGES_16][16];
extern uint8_t LCD_MESSAGE_20[MAX_PAGE_20x4][20];

extern uint16_t file_size;
extern uint16_t bytes_read;
extern uint8_t write_buffer[BUFF_SIZE];
extern uint8_t read_buffer[BUFF_SIZE];
extern uint8_t Display[DISPLAY_TYPE + 1];
extern uint8_t Config_Version_Partition[CONFIG_VER_SIZE+1];
extern uint8_t Config_Version_File[CONFIG_VER_SIZE+1];

extern bool Write_success;
extern bool Read_success;

#endif // MAIN_EEPROM_H