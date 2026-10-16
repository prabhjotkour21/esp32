#include "com.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"




struct COMM sIOT_COMM;
u8 DG_TXB[128];
u8 ACT[256];
u8 TXB[256];

size_t total_length  = 0;

int16_t VALUE[100] = {
	10,			//0
	30,			//1
	40,			//2
	80,			//3
	20,			//4
	10,			//5
	3100,		//6
	3110,		//7
	3120,		//8
	3130,		//9
	3140,		//10
	3150,		//11
	2000,		//12
	2001,		//13
	2002,		//14
	2003,		//15
	2004,		//16
	2005,		//17
	330,		//18
	333,		//19
	334,		//20
	338,		//21
	332,		//22
	331,		//23
	10,			//24
	-30,		//25
	40,			//26
	-80,		//27
	20,			//28
	-10,		//29
	3160,		//30
	-3170,		//31
	3180,		//32
	-3190,		//33
	3200,		//34
	-3210,		//35
};

u8 VAL8[6] =
{
	10,
	30,
	40,
	80,
	20,
	10,
};

u8 VALS8[6] =
{
	10,
	-30,
	40,
	-80,
	20,
	-10,
};

u16 VAL16[6] =
{
	3300,
	3330,
	3340,
	3380,
	3320,
	3310,
};

u16 VALS16[6] =
{
	3300,
	-3330,
	3340,
	-3380,
	3320,
	-3310,
};


u32 VAL32[6] =
{
	103300,
	103330,
	103340,
	103380,
	103320,
	103310,
};

float VALFLOAT[6] =
{
	3.30,
	3.33,
	3.34,
	3.38,
	3.32,
	3.31,
};

u8 VALSTRING[6][16] = {
	{"CHAL HAT"},
	{"JANE DE"},
	{"KUCH BHI"},
	{"CHAL KOI BAAT"},
	{"KYA CHALRAHA HA"},
	{"CHALNE DE"}
};


const struct sCOMM_DETAILS COMM_TABLES[MAX_COMM_TABLE][MAX_dCOMM_VALUES] = {
	{
		{3, 2, 0},
		{7, 2, 0},
		{14, 2, 0},
		{18, 2, 0},
		{25, 2, 0},
		{34, 2, 0},
		{1, 2, 0},
		{10, 2, 0},
		{16, 2, 0},
		{22, 2, 0},
		{COMM_DEND, 0, 0},
	},
	{
		{5, 2, 0},
		{8, 2, 0},
		{12, 2, 0},
		{20, 2, 0},
		{34, 2, 0},
		{37, 2, 0},
		{2, 2, 0},
		{11, 2, 0},
		{17, 2, 0},
		{19, 2, 0},
		{COMM_DEND, 0, 0},
	},
	{
		{1, 2, 0},
		{9, 2, 0},
		{15, 2, 0},
		{23, 2, 0},
		{30, 2, 0},
		{39, 2, 0},
		{4, 2, 0},
		{6, 2, 0},
		{13, 2, 0},
		{21, 2, 0},
		{COMM_DEND, 0, 0},
	},
	{
		{1, 2, 0},
		{11, 2, 0},
		{16, 2, 0},
		{19, 2, 0},
		{29, 2, 0},
		{30, 2, 0},
		{3, 2, 0},
		{8, 2, 0},
		{12, 2, 0},
		{22, 2, 0},
		{COMM_DEND, 0, 0},
	},

};


const void* FpCOMM_VAL[MAX_dCOMM_VALUES] = { // ONLY ADDRESSES WITHOUT COINSIDERATION FOR SIZE
	&VALUE[0],
	&VALUE[1],
	&VALUE[2],
	&VALUE[3],
	&VALUE[4],
	&VALUE[5],
	&VALUE[6],
	&VALUE[7],
	&VALUE[8],
	&VALUE[9],
	&VALUE[10],
	&VALUE[11],
	&VALUE[12],
	&VALUE[13],
	&VALUE[14],
	&VALUE[15],
	&VALUE[16],
	&VALUE[17],
	&VALUE[18],
	&VALUE[19],
	&VALUE[20],
	&VALUE[21],
	&VALUE[22],
	&VALUE[23],
	&VALUE[24],
	&VALUE[25],
	&VALUE[26],
	&VALUE[27],
	&VALUE[28],
	&VALUE[29],
	&VALUE[30],
	&VALUE[31],
	&VALUE[32],
	&VALUE[33],
	&VALUE[34],
	&VALUE[35],
	&VALUE[36],
	&VALUE[37],
	&VALUE[38],
	&VALUE[39],
	&VALUE[40],
	&VALUE[41],
	&VALUE[42],
	&VALUE[43],
	&VALUE[44],
	&VALUE[45],
	&VALUE[46],
	&VALUE[47],
	&VALUE[48],
	&VALUE[49],
	&VALUE[50],

};

const ID_Metadata_t id_metadata_table[MAX_IDS] = {
    [0] = {"A",  2, 1.0f, 1, TYPE_INT16},	// VAL8[0]
	[1] = {"B",  2, 1.0f, 1, TYPE_INT16},	// VAL8[0]
	[2] = {"C",  2, 1.0f, 1, TYPE_INT16},  // VAL8[0]
	[3] = {"D",  2, 1.0f, 1, TYPE_INT16},  // VAL8[0]
	[4] = {"E",  2, 1.0f, 1, TYPE_INT16},  // VAL8[0]
	[5] = {"F",  2, 1.0f, 1, TYPE_INT16},  // VAL8[0]
	
	[6] = {"G",  2, 1.0f, 1, TYPE_INT16},
	[7] = {"H",  2, 1.0f, 1, TYPE_INT16},
	[8] = {"I",  2, 1.0f, 1, TYPE_INT16},
	[9] = {"J",  2, 1.0f, 1, TYPE_INT16},
	[10] = {"K", 2, 1.0f, 1, TYPE_INT16},
    [11] = {"L", 2, 1.0f, 1, TYPE_INT16},
    
    [12] = {"M", 2, 1.0f, 1, TYPE_INT16},
    [13] = {"N", 2, 1.0f, 1, TYPE_INT16},
    [14] = {"O", 2, 1.0f, 1, TYPE_INT16},
    [15] = {"P", 2, 1.0f, 1, TYPE_INT16},
    [16] = {"Q", 2, 1.0f, 1, TYPE_INT16},
    [17] = {"R", 2, 1.0f, 1, TYPE_INT16},
    
    [18] = {"S", 2, 1.0f, 2, TYPE_INT16},
    [19] = {"T", 2, 1.0f, 2, TYPE_INT16},
    [20] = {"U", 2, 1.0f, 2, TYPE_INT16},
    [21] = {"V", 2, 1.0f, 2, TYPE_INT16},
    [22] = {"W", 2, 1.0f, 2, TYPE_INT16},
    [23] = {"X", 2, 1.0f, 2, TYPE_INT16},

	[30] = {"EE", 2, 1.0f, 1, TYPE_INT16},
	[31] = {"FF", 2, 1.0f, 1, TYPE_INT16},
	[32] = {"II", 2, 1.0f, 1, TYPE_INT16},
	[34] = {"KK", 2, 1.0f, 1, TYPE_INT16},
	[35] = {"LL", 2, 1.0f, 1, TYPE_INT16},
	
	[36] = {"MM", 2, 1.0f, 1, TYPE_INT16},
	[37] = {"NN", 2, 1.0f, 1, TYPE_INT16},
	[38] = {"OO", 2, 1.0f, 1, TYPE_INT16},
	[39] = {"PP", 2, 1.0f, 1, TYPE_INT16},
	[40] = {"QQ", 2, 1.0f, 1, TYPE_INT16},
};


//=====================[CRC Logic]==========================
u16 FIND_CRC(u16 CRC_VAL, u16 DATA_BYTE) {
	u32 J;
	CRC_VAL ^= (u16) DATA_BYTE;
	for (J = 0; J < 8; ++J) {
		DATA_BYTE = (CRC_VAL & 1);
		CRC_VAL >>= 1;
		if (DATA_BYTE) {
			CRC_VAL ^= CRC_POLY;
		}
	}
	return CRC_VAL;
}


//======================[Transmitter]======================
u8 Transmitter1 (struct COMM *S) {
	u32 TX_D;
	u32 TX_PACKET_SIZE;

	if (!(*S).TX_STEP)
	{
		(*S).TX_STEP = 1;
		TX_D = START_CHARACTER;
		(*S).TX_MSB = 0;
		(*S).TX_DATA_STEP = 0;
		(*S).TX_DATA_INDEX_COUNTER = 0;
		//(*S).TX_SEND_MSB = 0;
		CRC_INI((*S).TX_CRC);
		goto J_LD_DATAwithCRC;
	}
	TX_PACKET_SIZE = (GET_PACKET_SIZE((*S).TX_DATA_MAX)+P_DATA_START);
	// printf("Packet Size : %ld", TX_PACKET_SIZE);
	TX_D = (*S).TX_STEP++;
	if (TX_D == P_PACKET_DESTINATION)
		TX_D = (*S).TX_DESTINATION;
	else if (TX_D == P_PACKET_SOURCE)
		TX_D = (*S).TX_SOURCE; // MY_ID;
	else if (TX_D == P_PACKET_TYPE)
		TX_D = (*S).TX_TYPE;
	else if (TX_D == P_TABLE_N)
		TX_D = (*S).TX_TABLE_N;
	else if (TX_D == P_DATA_SIZE)
		TX_D = TX_PACKET_SIZE - P_DATA_START;
	else if ((*S).TX_SEND_MSB_NOW) // **REQUIRE > 127 FOR CRC TX_D & (1<<7)) // MSB
	{
		TX_D = (*S).TX_MSB;
		(*S).TX_MSB = 0;
		(*S).TX_SEND_MSB_NOW = 0; // TX_STEP &= ~(1<<7);
	}
	else if (TX_D < TX_PACKET_SIZE)
	{	u32 I, DD;
		I = (*S).TX_DATA_STEP;
		TX_D = (*S).TX_DATA[I];
		(*S).TX_MSB <<= 1;
		if (TX_D & 0x80)
			(*S).TX_MSB |= 1;

		I = ++(*S).TX_DATA_STEP;
		if ((I == (*S).TX_DATA_MAX) || !(I % 7))
			(*S).TX_SEND_MSB_NOW = 1; // **REQUIRE > 127 FOR CRC (*S).TX_STEP |= (1<<7); // MSB

		DD = (*S).TX_DATA_INDEX_COUNTER;
		if (I == (((*S).TX_DATA_INDEX_POSITIONS[DD])+1U)) // +1 BECAUSE VALUE ALREADY INCREMENTED
		{
			(*S).TX_DATA_INDEX_COUNTER++;
			TX_D <<= 1;
			goto J_LD_DATAwithCRC;
		}
	}
	else if (TX_D == TX_PACKET_SIZE)
	{
		TX_D = (u8)(*S).TX_CRC & 0xFF;
		goto J_LD_CRC;
	}
	else if (TX_D == (TX_PACKET_SIZE+1))
	{
		TX_D = (*S).TX_CRC;
		(*S).TX_MSB = 0;
		if (TX_D & 0x80)
			(*S).TX_MSB = (1<<1);
		if (TX_D & 0x8000)
			(*S).TX_MSB |= (1<<0);
		TX_D >>= 8;
		goto J_LD_CRC;
	}
	else
	{
		(*S).BF_TRANSMIT = 0;
		(*S).TX_STEP = 0;
		TX_D = (*S).TX_MSB;
J_LD_CRC:
		TX_D = (u8)((TX_D << 1) | 1);
		goto J_LD_DATA;
	}
	TX_D = ((TX_D << 1) | 1 );
J_LD_DATAwithCRC:
	TX_D &= 0xFF;
	(*S).TX_CRC = FIND_CRC((*S).TX_CRC, TX_D);
J_LD_DATA:
	return TX_D;
}


void TRANSMIT (u8 COMM_N)
{
	sIOT_COMM.TX_DESTINATION = 1;
	sIOT_COMM.TX_SOURCE = 2;
	sIOT_COMM.TX_TYPE = 1;
	sIOT_COMM.BF_TRANSMIT = 1;

	sIOT_COMM.TX_TABLE_N = COMM_N;
	LOAD_COMM_BYTES(&sIOT_COMM);

	{	u32 I = 0;
		while (sIOT_COMM.BF_TRANSMIT)
		{
			DG_TXB[I++] = Transmitter1(&sIOT_COMM);
		}
		printf("Data Index : %d\n", sIOT_COMM.TX_DATA_INDEX_COUNTER);
	}
}

void LOAD_COMM_BYTES(struct COMM *S)
{
	u32 I, N, BYTEN, TABLEN;
	struct sCOMM_DETAILS COMS;
	u8 *D, *P;

	D = &((*S).TX_DATA[0]);
	(*S).TX_DATA_MAX = 0;
	(*S).TX_DATA_INDEX_COUNTER = 0;
	TABLEN = (*S).TX_TABLE_N;
	for (I = 0; I < MAX_dCOMM_VALUES; ++I)
	{
	    BYTEN = COMM_TABLES[TABLEN][I].DATA_I;
		if (BYTEN == COMM_DEND)
			break;
		COMS = COMM_TABLES[TABLEN][I];
		BYTEN = COMS.DATA_SIZE;
		N = (*S).TX_DATA_INDEX_COUNTER++;
		(*S).TX_DATA_INDEX_POSITIONS[N] = (*S).TX_DATA_MAX;
		*D++ = N;
		// *D++ = COMS.DATA_I;
		(*S).TX_DATA_MAX += (BYTEN+1U); // EXTRA FOR ID
		P = (u8 *) FpCOMM_VAL[COMS.DATA_I];
		do
		{
			*D++ = *P++;
		} while (--BYTEN);
	}
}

//==========================[Receiver]=======================
void Receiver1(struct COMM *S, u32 RXDATA) {
	u32 DUMMY, PACKET_I;
	DUMMY = (RXDATA >> 1);
	PACKET_I = (*S).RX_STEP;

	if (RXDATA == START_CHARACTER) {
		(*S).RX_DATA_STEP = 0;
		(*S).RX_STEP = 0;
		(*S).RX_DATA_INDEX_COUNTS = 0;
		CRC_INI((*S).RX_CRC);
		goto J_RX_NXT;
	} else if (!PACKET_I) {
		return;
	}

	if (PACKET_I == P_PACKET_DESTINATION) {
		(*S).RX_DESTINATION = DUMMY;
	} else if (PACKET_I == P_PACKET_SOURCE) {
		(*S).RX_SOURCE = DUMMY;
	} else if (PACKET_I == P_PACKET_TYPE) {
		(*S).RX_TYPE = DUMMY;
	} else if (PACKET_I == P_TABLE_N) {
		(*S).RX_TABLE_N = DUMMY;
	} else if (PACKET_I == P_DATA_SIZE) {
		(*S).RX_PACKET_SIZE = DUMMY + P_DATA_START;
		(*S).RX_DATA_MAX = (DUMMY - (u32)(DUMMY + 7U) / 8U);
	} else if (PACKET_I < (*S).RX_PACKET_SIZE) {
		u32 J = 0;
		PACKET_I = ((*S).RX_DATA_STEP);
		if (!(RXDATA & 1)) {
			J = (*S).RX_DATA_INDEX_COUNTS++;
			(*S).RX_DATA_INDEX_POSITIONS[J] = (PACKET_I & 0x7F);
			// printf("Packet_I : %ld\n", PACKET_I);
		}
		if (!PACKET_I)
			goto JLD_D;
		if (PACKET_I & (1 << 7)) {
			PACKET_I &= ~(1 << 7);
			goto JLD_D;
		}
		J = PACKET_I % 7;
		if (!J) {
			goto JRXMSB;
		} else if (PACKET_I == (*S).RX_DATA_MAX) {
			DUMMY &= (0xFF >> (8 - J));
JRXMSB:
			while (DUMMY) {
				--PACKET_I;
				if (DUMMY & 1)
					(*S).RX_DATA[PACKET_I] |= 0x80;
				DUMMY >>= 1;
			}
			(*S).RX_DATA_STEP |= (1 << 7);
		} else {
JLD_D:
			(*S).RX_DATA[PACKET_I] = DUMMY;
			(*S).RX_DATA_STEP = ++PACKET_I;
		}
	} else if (PACKET_I == (*S).RX_PACKET_SIZE) {
		(*S).RX_CS_RECEIVED.BYTE.B0 = DUMMY;
		goto J_RX_NXT0;
	} else if (PACKET_I == ((*S).RX_PACKET_SIZE + 1)) {
		(*S).RX_CS_RECEIVED.BYTE.B1 = DUMMY;
		goto J_RX_NXT0;
	} else {
		PACKET_I = (*S).RX_CS_RECEIVED.HWORD;
		if (DUMMY & 2)
			PACKET_I |= 0x80;
		if (DUMMY & 1)
			PACKET_I |= 0x8000;
		(*S).RX_CS_RECEIVED.HWORD = PACKET_I;

		if (PACKET_I == (*S).RX_CRC)
			(*S).RX_DATA_RECEIVED = 1;
		else
			(*S).RX_COMM_ERR = 1;
		(*S).RX_STEP = 0;
		return;
	}
J_RX_NXT:
	(*S).RX_CRC = FIND_CRC((*S).RX_CRC, RXDATA);
J_RX_NXT0:
	++(*S).RX_STEP;
	return;
}


//===========================[Helper Functions]===========================
void putNibble(uint8_t *ptr, int8_t val){
	if((val <= 9) && (val >= 0)){
        *ptr = val + '0';
    }
	else *ptr = val -10 + 'A';
}

void putByte(uint8_t *ptr, uint8_t val){
	putNibble(ptr, val/16);
	putNibble(ptr+1, val%16);
}
u8 PRODUCT_VERSION[PRODUCT_VERSION_SIZE] = {'I','N','V','3','P','0','0','1'};
// Process Task
void Process_data(void *arg){
    (void)arg;
    u8 table = 0;
    u8 source = 0x01;
    u8 destination = 0x02;
    u8 Packet_type = 0x03;

    while(1){
		vTaskDelay(pdMS_TO_TICKS(3000));
		// raw_packet array under study
		TRANSMIT(0); 
		
		size_t packet_length = ((DG_TXB[5] >> 1) + 9);
        if (packet_length > sizeof(DG_TXB)) {
            printf("Generated packet length %u exceeds transmit buffer\n",
                   (unsigned)packet_length);
            continue;
        }
	// 	packet_length = sizeof(raw_packet) / sizeof(raw_packet[0]);
		// Packet Receiving
		printf("--- Streaming Packet into Receiver1 State Machine ---\n");
		for (size_t i = 0; i < packet_length; i++) {
			Receiver1(&sIOT_COMM, DG_TXB[i]);
			// printf("Index [%02lu] : 0x%02X\n", (unsigned long)i, DG_TXB[i]);
			// printf("Index [%02lu] : 0x%02X\n", (unsigned long)i, sIOT_COMM.TX_DATA[i]);
			// printf("ID Location [%02X]\n", sIOT_COMM.TX_DATA_INDEX_POSITIONS[i]);
		}

        if(sIOT_COMM.RX_DATA_RECEIVED && !sIOT_COMM.RX_COMM_ERR){
            printf("Data Received Successfully.\n");
            packet_length = sIOT_COMM.RX_DATA_MAX;
        }
        else {
            printf("Data Received Failed.\n");
            sIOT_COMM.RX_DATA_RECEIVED = false;
            sIOT_COMM.RX_COMM_ERR = false;
            continue;
        }

        if (packet_length > sizeof(sIOT_COMM.RX_DATA) || sIOT_COMM.RX_DATA_INDEX_COUNTS > MAX_IDinPACKET) {
            printf("Decoded packet exceeds receive buffer limits\n");
            sIOT_COMM.RX_DATA_RECEIVED = false;
            sIOT_COMM.RX_COMM_ERR = false;
            continue;
        }

        total_length = (packet_length * 2) + P_HEADER_SIZE + sIOT_COMM.RX_DATA_INDEX_COUNTS;
        if (total_length + 4 > sizeof(TXB) ||
            total_length > sizeof(ACT) ||
            total_length > UINT8_MAX) {
            printf("Encoded packet length %u exceeds server packet limits\n",
                   (unsigned)total_length);
            sIOT_COMM.RX_DATA_RECEIVED = false;
            sIOT_COMM.RX_COMM_ERR = false;
            continue;
        }

		// for (size_t i = 0; i < (packet_length); i++) {
		// 	printf("Index [%02lu]: 0x%02X\n",(unsigned long)i,sIOT_COMM.RX_DATA[i]);
		// }
		// printf("\n");
		app_command_t command = {0};
		if (xQueueReceive(s_app_command_queue, &command, 0) == pdTRUE) {
			// printf("Received app command: type=%d request_id=%lu table=%s version=%s\n",
			//        command.type,
			//        (unsigned long)command.request_id,
			//        command.table_number,
			//        command.version);
			if(command.packet_type == Display_Packet_TYPE){
				memset(ACT, 0x00, 256);
				ACT[0] = APP_START_CHARACTER;
				ACT[1] = APP_START_CHARACTER;
				for(size_t i = 0, j = 2; i < PRODUCT_VERSION_SIZE; i++, j+=2){
					putByte(&ACT[j], (uint8_t)PRODUCT_VERSION[i]);
				}
				putByte(&ACT[18], (uint8_t)ESP32);
				putByte(&ACT[20], (uint8_t)APP);
				putByte(&ACT[22], (uint8_t)Display_Packet_TYPE);
				putByte(&ACT[24], (uint8_t)Display_Packet_TYPE);
				putByte(&ACT[26], (uint8_t)((packet_length*2)+sIOT_COMM.RX_DATA_INDEX_COUNTS+2));
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
					continue;
				}
				putByte(&ACT[total_length], (uint8_t)(VALID_PACKET));
				u16 CRC = 0xFFFF;
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
				memcpy(send_packet.data, TXB, send_packet.length);

				app_response_t response = {0};
				response.request_id = command.request_id;
				response.success = true;
				response.packet = send_packet;
				if (xQueueSend(s_app_response_queue, &response, 0) != pdTRUE) {
					printf("Failed to queue outbound server packet\n");
				} else {
					printf("Queued outbound server packet of length %u\n", (unsigned)response.packet.length);
				}
				// printf("Server Packet\n");
				// for (size_t i = 0; i < ((total_length)+6); i++) {
				// 	printf("0x%02X,\n",TXB[i]);
				// }

			}
		}

        // printf("Packet Length : %d\n", packet_length);
        // printf("RX Index : %d\n", sIOT_COMM.RX_DATA_INDEX_COUNTS);
        // printf("Actul packet\n");
        // for(size_t i = 0; i < (packet_length); i++){
        //     printf("0x%02X\n",sIOT_COMM.RX_DATA[i]);
        // }
        // printf("\n");
        // printf("Data ID Position\n");
        // for(size_t i = 0; i < (sIOT_COMM.RX_DATA_INDEX_COUNTS); i++){
        //     printf("0x%02X\n",sIOT_COMM.RX_DATA_INDEX_POSITIONS[i]);
        // }
        // printf("\n");
        // bool layout_valid = true;
		// for(size_t i = P_HEADER_SIZE, j = 0, k = 0, cnt = 0; i < total_length; ){
		// 	if(k < sIOT_COMM.RX_DATA_INDEX_COUNTS &&
        //        i == (sIOT_COMM.RX_DATA_INDEX_POSITIONS[k] + P_HEADER_SIZE + cnt + j)){
		// 		ACT[i] = '$';
		// 		cnt++;
		// 		k++;
		// 		i++;
		// 	}
		// 	else{
        //         if (j >= packet_length || i + 1 >= total_length) {
        //             printf("Invalid encoded packet layout\n");
        //             layout_valid = false;
        //             break;
        //         }
        //         putByte(&ACT[i], sIOT_COMM.RX_DATA[j]);
		// 		i+=2;
		// 		j++;
		// 	}
		// }
        // if (!layout_valid) {
        //     sIOT_COMM.RX_DATA_RECEIVED = false;
        //     sIOT_COMM.RX_COMM_ERR = false;
        //     continue;
        // }
        // printf("\n");
		// u16 CRC = 0xFFFF;
		// for (size_t i = 0; i < ((packet_length * 2) + P_HEADER_SIZE + sIOT_COMM.RX_DATA_INDEX_COUNTS); i++) {
		// 	// printf("0x%02X\n",ACT[i]);
		// 	CRC = FIND_CRC(CRC, ACT[i]);
		// }
        // printf("\n");
		// printf("CRC : [0x%02X], Length = %d\n", CRC, total_length);
		// printf("\n");
        // memset(TXB, 0x00, 256);
        // memcpy(TXB, ACT, total_length);
        // putByte(&TXB[(total_length)], (uint8_t)(CRC >> 8) & 0xFF);
        // putByte(&TXB[(total_length)+2], (uint8_t)(CRC >> 0) & 0xFF);

        
        // printf("Server Packet\n");
        // for (size_t i = 0; i < ((total_length)+4); i++) {
		// 	printf("0x%02X,\n",TXB[i]);
		// }

		// server_packet_t send_packet = {0};
		// send_packet.length = (total_length + 4);
		// memcpy(send_packet.data, TXB, send_packet.length);
		// if (xQueueSend(s_server_packet_queue, &send_packet, 0) != pdTRUE) {
		// 	printf("Failed to queue outbound server packet\n");
		// }
			
        // if (!queue_server_packet(TXB, total_length + 4)) {
        //     printf("Could not queue packet for TCP server\n");
        // }

		// printf("\n---- TX_DATA_INDEX_COUNTER = [%02d]\n", sIOT_COMM.TX_DATA_INDEX_COUNTER);
		// printf("\n--- Validation Summary ---\n");
		// printf("RX_DATA_RECEIVED Status Flag: %s\n", sIOT_COMM.RX_DATA_RECEIVED ? "VALIDATED (1)" : "INVALID/INCOMPLETE (0)");
		// printf("RX_COMM_ERR Status Flag:     %s\n", sIOT_COMM.RX_COMM_ERR ? "CRC ERROR DETECTED (1)" : "CLEAN (0)");

		// printf("\n--- Evaluated Fields ---\n");
		// printf("Calculated RX CRC:  0x%04X\n", sIOT_COMM.RX_CRC);
		// printf("Calculated TX CRC:  0x%04X\n", sIOT_COMM.TX_CRC);
		// printf("Reconstructed target CRC: 0x%04X\n", sIOT_COMM.RX_CS_RECEIVED.HWORD);
		
		// printf("[DECONSTRUCTED TELEMETRY VALUE PAYLOAD]\n");
		// u8 vID = 0;
		// u8 bSize = 0;
		// u8 uTYPE = 0;
		// u8 x = 0; 
		// float actualVal = 0;
		
		// for (u32 d = 0; d < sIOT_COMM.RX_DATA_INDEX_COUNTS; d++) {
		// 	x = sIOT_COMM.RX_DATA_INDEX_POSITIONS[d];
		// 	vID = sIOT_COMM.RX_DATA[x];
			
		// 	// if (vID >= MAX_IDS || id_metadata_table[vID].name == NULL) {
		// 	// 	printf("Index[%lu] : Unknown ID [%d]\n", (unsigned long)x, vID);
		// 	// 	continue;
		// 	// }
		
		// 	bSize = id_metadata_table[vID].num_bytes;
		// 	uTYPE = id_metadata_table[vID].data_type;
			
		// 	if((bSize == 1)&&(uTYPE == TYPE_UINT8)){
		// 		uint8_t uVal = ((uint8_t)sIOT_COMM.RX_DATA[x + 1]);
		// 		actualVal = (float)uVal * id_metadata_table[vID].multiplier;
				
		// 		printf("Index[%2lu] | ID 0x%02X | %-16s = %.*f\n", 
		// 			(unsigned long)x, vID, id_metadata_table[vID].name, 
		// 			id_metadata_table[vID].decimal_place, actualVal);
		// 	}
			
		// 	if((bSize == 1)&&(uTYPE == TYPE_INT8)){
		// 		int8_t sVal = ((int8_t)sIOT_COMM.RX_DATA[x + 1]);
		// 		actualVal = (float)sVal * id_metadata_table[vID].multiplier;
				
		// 		printf("Index[%2lu] | ID 0x%02X | %-16s = %.*f\n", 
		// 			(unsigned long)x, vID, id_metadata_table[vID].name, 
		// 			id_metadata_table[vID].decimal_place, actualVal);
		// 	}
			
		// 	if ((bSize == 2)&&(uTYPE == TYPE_UINT16)) {
		// 		uint16_t rawVal = ((uint16_t)sIOT_COMM.RX_DATA[x + 2] << 8) | ((uint16_t)sIOT_COMM.RX_DATA[x + 1]);
		// 		actualVal = (float)rawVal * id_metadata_table[vID].multiplier;
				
		// 		printf("Index[%2lu] | ID 0x%02X | %-16s = %.*f\n", 
		// 			(unsigned long)x, vID, id_metadata_table[vID].name, 
		// 			id_metadata_table[vID].decimal_place, actualVal);
		// 	}
			
		// 	if ((bSize == 2)&&(uTYPE == TYPE_INT16)) {
		// 		uint16_t rawBits = ((uint16_t)sIOT_COMM.RX_DATA[x + 2] << 8) | ((uint16_t)sIOT_COMM.RX_DATA[x + 1]);
		// 		int16_t sVal = (int16_t)rawBits;
		// 		actualVal = (float)sVal * id_metadata_table[vID].multiplier;
				
		// 		printf("Index[%2lu] | ID 0x%02X | %-16s = %.*f\n", 
		// 			(unsigned long)x, vID, id_metadata_table[vID].name, 
		// 			id_metadata_table[vID].decimal_place, actualVal);
		// 	}
			
		// 	if((bSize == 4)&&(uTYPE == TYPE_UINT32)){
		// 		uint32_t uVal = ((uint32_t)sIOT_COMM.RX_DATA[x + 4] << 24) |
		// 				((uint32_t)sIOT_COMM.RX_DATA[x + 3] << 16) |
		// 				((uint32_t)sIOT_COMM.RX_DATA[x + 2] << 8)  |
		// 				((uint32_t)sIOT_COMM.RX_DATA[x + 1]);
						
		// 		actualVal = (float)uVal * id_metadata_table[vID].multiplier;
				
		// 		printf("Index[%2lu] | ID 0x%02X | %-16s = %.*f\n", 
		// 		(unsigned long)x, vID, id_metadata_table[vID].name, 
		// 		id_metadata_table[vID].decimal_place, actualVal);
				
		// 	}
			
		// 	if((bSize == 4)&&(uTYPE == TYPE_INT32)){
		// 		uint32_t rawBits = ((uint32_t)sIOT_COMM.RX_DATA[x + 4] << 24) |
		// 				((uint32_t)sIOT_COMM.RX_DATA[x + 3] << 16) |
		// 				((uint32_t)sIOT_COMM.RX_DATA[x + 2] << 8)  |
		// 				((uint32_t)sIOT_COMM.RX_DATA[x + 1]);
						
		// 		int32_t sVal = (int32_t)rawBits;
		// 		actualVal = (float)sVal * id_metadata_table[vID].multiplier;
				
		// 		printf("Index[%2lu] | ID 0x%02X | %-16s = %.*f\n", 
		// 		(unsigned long)x, vID, id_metadata_table[vID].name, 
		// 		id_metadata_table[vID].decimal_place, actualVal);
				
		// 	}
			
		// 	if((bSize == 4) && (uTYPE == TYPE_FLOAT)){
		// 		uint32_t rawBits = ((uint32_t)sIOT_COMM.RX_DATA[x + 4] << 24) |
		// 				((uint32_t)sIOT_COMM.RX_DATA[x + 3] << 16) |
		// 				((uint32_t)sIOT_COMM.RX_DATA[x + 2] << 8)  |
		// 				((uint32_t)sIOT_COMM.RX_DATA[x + 1]);
		// 		printf("RAW Float Value [%ld]", rawBits);
						
		// 		float fVal;
		// 		memcpy(&fVal, &rawBits, sizeof(fVal));
		// 		actualVal = fVal * id_metadata_table[vID].multiplier;
				
		// 		printf("Index[%2lu] | ID 0x%02X | %-16s = %.*f\n", 
		// 		(unsigned long)x, vID, id_metadata_table[vID].name, 
		// 		id_metadata_table[vID].decimal_place, actualVal);
		// 	}
			
		// }

		// if (sIOT_COMM.RX_DATA_RECEIVED) {
		// 	printf("\n--- Extracted Header ---\n");
		// 	printf("Destination: 0x%02X | Source: 0x%02X | Type: 0x%02X | Table: %d\n",
		// 		sIOT_COMM.RX_DESTINATION, sIOT_COMM.RX_SOURCE, sIOT_COMM.RX_TYPE, sIOT_COMM.RX_TABLE_N);
		// 	printf("Payload Length: %d Bytes\n", sIOT_COMM.RX_DATA_MAX);
		// 	printf("Payload Length HEX: %X Bytes\n", ((sIOT_COMM.RX_DATA_MAX + sIOT_COMM.RX_DATA_INDEX_COUNTS + 6 + 2)<< 1));
		// 	printf("Index Count: %d Bytes\n", sIOT_COMM.RX_DATA_INDEX_COUNTS);
		// }
        sIOT_COMM.RX_DATA_RECEIVED = false;
        sIOT_COMM.RX_COMM_ERR = false;
		table++;
		if(table >= MAX_COMM_TABLE)
			table = 0;
	}
}