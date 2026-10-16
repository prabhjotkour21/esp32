#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "cJSON.h"
#include "esp_event.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "mdns.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "lwip/inet.h"
#include "lwip/sockets.h"
#include "nvs.h"
#include "nvs_flash.h"

#define START_CHARACTER 		0xFE
#define APP_START_CHARACTER		'#'
#define PRODUCT_VERSION_SIZE	8U

#define MAX_dCOMM_VALUES	    100
#define MAX_IDS 				255
#define MAX_COMM_TABLE			4
#define COMM_DEND    			((u16)0xFFFF)
#define CRC_POLY                0xA001
#define CRC_INI(x)              {(x) = 0xFFFF;}

#define P_PACKET_START          0U
#define P_PACKET_DESTINATION    1U
#define P_PACKET_SOURCE         2U
#define P_PACKET_TYPE           3U
#define P_TABLE_N               4U
#define P_DATA_SIZE             5U
#define P_DATA_START            6U
#define CS_BYTES_COUNT          3U
#define P_HEADER_SIZE			28U

#define GET_PACKET_SIZE(x)		((u32)((x)+(u32)(((x)+6U)/7U)))

#define MAX_COMM_BUF            128
#define MAX_IDinPACKET          (MAX_COMM_BUF>>1)

typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;

struct sCOMM_DETAILS {
	u16	DATA_I;
	u8	DATA_SIZE;
	u8	FREE;
};

union TwoBytes {
	u16 HWORD;
	struct {
		u8 B0;
		u8 B1;
	} BYTE;
};

typedef enum {
    TYPE_UINT8,
    TYPE_INT8,
    TYPE_UINT16,
    TYPE_INT16,
    TYPE_UINT32,
    TYPE_INT32,
    TYPE_FLOAT,
    TYPE_STRING
} DataType_e;

typedef struct {
    const char* name;      
    uint8_t num_bytes;     
    float multiplier;      
    uint8_t decimal_place; 
    uint8_t data_type;
} ID_Metadata_t;


struct COMM {
	u8      RX_DESTINATION;
	u8      RX_SOURCE;
	u8      RX_TYPE;
	u8      RX_TABLE_N;
	u8      RX_PACKET_SIZE;
	u8      RX_STEP;
	u8      RX_DATA_STEP;
	u8      RX_DATA_MAX;

	u8      RX_DATA_INDEX_COUNTS;
	u8      RX_DATA_INDEX_POSITIONS[MAX_IDinPACKET];
	u8      RX_DATA[MAX_COMM_BUF];
	bool    RX_DATA_RECEIVED;
	bool    RX_COMM_ERR;

	u16     RX_CRC;
	union TwoBytes RX_CS_RECEIVED;

	u8      TX_STEP;
	u8      TX_DESTINATION;
	u8      TX_SOURCE;
	u8      TX_TYPE;
	u8      TX_TABLE_N;
	u8      TX_DATA_MAX;
	u8      TX_DATA_STEP;
	u8      TX_MSB;
	u8      TX_DATA_INDEX_COUNTER;
	u8      TX_DATA_INDEX_POSITIONS[MAX_IDinPACKET];
	u8      TX_DATA[MAX_COMM_BUF];

	u16     TX_CRC;
	bool    TX_SEND_MSB_NOW;
	bool    BF_TRANSMIT;
};


extern int16_t VALUE[100];

extern u8 VAL8[6];
extern u8 VALS8[6];
extern u16 VAL16[6];
extern u16 VALS16[6];
extern u32 VAL32[6];
extern float VALFLOAT[6];
extern u8 VALSTRING[6][16];

extern struct COMM sIOT_COMM;
extern u8 DG_TXB[128];
extern u8 ACT[256];
extern u8 TXB[256];

extern const ID_Metadata_t id_metadata_table[MAX_IDS];
extern const void* FpCOMM_VAL[MAX_dCOMM_VALUES];
extern const struct sCOMM_DETAILS COMM_TABLES[MAX_COMM_TABLE][MAX_dCOMM_VALUES];

extern u8 PRODUCT_VERSION[PRODUCT_VERSION_SIZE];


// Function Declaration
void TRANSMIT (u8 COMM_N);
void LOAD_COMM_BYTES(struct COMM *S);
u8 Transmitter1 (struct COMM *S);
void Receiver1(struct COMM *S, u32 RXDATA);
u16 FIND_CRC(u16 CRC_VAL, u16 DATA_BYTE);


void putNibble(uint8_t *ptr, int8_t val);
void putByte(uint8_t *ptr, uint8_t val);
void Process_data(void *arg);
bool queue_server_packet(const uint8_t *data, size_t length);



#define SETUP_AP_SSID "ESP32_PORT"
#define SETUP_AP_PASSWORD "123456789"
#define APP_SERVER_PORT 8080
#define PROVISIONING_MAX_BODY 512
#define WIFI_CONNECTED_BIT BIT0
#define TCP_RETRY_DELAY_MS 3000
#define TCP_ACK_TIMEOUT_MS 3000
#define TCP_MAX_PACKET_RETRIES 5
#define MDNS_QUERY_TIMEOUT_MS 3000

#define SERVER_PACKET_MAX_SIZE 256
#define SERVER_PACKET_QUEUE_LENGTH 5
#define APP_COMMAND_QUEUE_LENGTH 5
#define DEVICE_ID "ESP32_0001"

typedef struct {
    size_t length;
    uint8_t data[SERVER_PACKET_MAX_SIZE];
} server_packet_t;

typedef struct {
    uint32_t request_id;
    bool success;
    server_packet_t packet;
} app_response_t;

extern size_t total_length;
extern QueueHandle_t s_server_packet_queue;
extern QueueHandle_t s_app_command_queue;
extern QueueHandle_t s_app_response_queue;