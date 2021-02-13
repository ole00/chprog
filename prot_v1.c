#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

extern uint8_t chipID;
extern uint8_t u8Buff[64];

/* text: USB DBG CH559 & ISP*/
static char detect_chip_cmd_v1[64] = {0xa2, 0x13, 0x55, 0x53, 0x42, 0x20, 0x44, 0x42,
				   0x47, 0x20, 0x43, 0x48, 0x35, 0x35, 0x39, 0x20,
				   0x26, 0x20, 0x49, 0x53, 0x50, 0x00};

static char use_interface_cmd_v1[2] = {0xbb, 0x00};

static char key_input_cmd_v1[6] = {0xa6, 0x04, 0x00, 0x00, 0x00, 0x00};

static char erase_page_cmd_v1[4] = {0xa9, 0x02, 0x00, 0x00}; /* 1KB/Page? */
static char write_cmd_v1[64] = {0xa8, 0x00, 0x00, 0x00}; /* cmd, length, addrl, addrh, data */
static char verify_cmd_v1[64] = {0xa7, 0x00, 0x00, 0x00}; /* cmd, length, addrl, addrh, data */
static char run_cmd_v1[4] = {0xa5, 0x02, 0x01, 0x00};

uint32_t Write(uint8_t *p8Buff, uint8_t u8Length);
uint32_t Read(uint8_t *p8Buff, uint8_t u8Length);

int v1_detect(void) {

	/* Detect MCU */
	if (!Write(detect_chip_cmd_v1, 63)) {
		printf("Send Detect: Fail\n");
		return 1;
	}

	if (!Read(u8Buff, 2)) {
		printf("Read Detect: Fail\n");
		return 1;
	}

	chipID = u8Buff[0];
	printf("Chip ID=%02x\n", chipID);
	
	/* Detect Bootloader */
	if (!Write(use_interface_cmd_v1, 2)){
		printf("Send ID: Fail\n");
		return 1;
	}
	if (!Read(u8Buff, 2)) {
		printf("Read ID: Fail\n");
		return 1;
	}
	printf("Bootloader: %d.%d\n", u8Buff[0] >> 4, u8Buff[0] & 0xf);
	//hexdump(inbuffer, 2);

	/* Input a dummy key for non encrypted FW */
	if (!Write(key_input_cmd_v1, 6)) {
		printf("Send Key: Fail\n");
		return 1;
	}
	if (!Read(u8Buff, 2)) {
		printf("Read Key: Fail\n");
		return 1;
	}
	return 0;
}

int v1_write(uint8_t* file_buffer, int file_length) {
	int i;
	int n;
	int curr_addr = 0;
	int pkt_length;

	/* Erase */
	n = (file_length + 1023) / 1024;
	for(i = 0; i < n ; i++)
	{
		erase_page_cmd_v1[3] = i * 4;
		if (!Write(erase_page_cmd_v1, 4)) {
			printf("Send Erase: Fail\n");
			return 1;
		}
		if (!Read(u8Buff, 2)) {
			printf("Read Erase: Fail\n");
			return 1;
		}

		if(u8Buff[0] != 0x00)
		{
			fprintf(stderr, "Erase failed!\n");
			return 1;
		}
	}
	
	
	/* Write */
	i = file_length;

	while(curr_addr < file_length) {
		pkt_length = i >= 0x3c? 0x3c: i;
		write_cmd_v1[1] = pkt_length;
		write_cmd_v1[2] = curr_addr & 0xff;
		write_cmd_v1[3] = (curr_addr >> 8) & 0xff;

		memcpy(write_cmd_v1 + 4, file_buffer + curr_addr, pkt_length);
		if(!Write(write_cmd_v1, 64)) {
			printf("Send Write: Fail\n");
			return 1;
		}
		if (!Read(u8Buff, 2)) {
			printf("Read Write: Fail\n");
			return 1;
		}
		//hexdump(u8Buff, 2);

		//printf("Write to addr %d, pkt_length %d\n", curr_addr, pkt_length);

		curr_addr += pkt_length;
		i -= pkt_length;

		if(u8Buff[0] != 0x00)
		{
			printf("Error: Write failed!\n");
			return 1;
		}
		
	}
	return 0;
}
	
int v1_verify(uint8_t* file_buffer, int file_length) {
	int i;
	int curr_addr = 0;
	int pkt_length;
	
	/* Verify */
	i = file_length;
	curr_addr = 0;
	while(curr_addr < file_length) {
		pkt_length = i >= 0x3c? 0x3c: i;
		verify_cmd_v1[1] = pkt_length;
		verify_cmd_v1[2] = curr_addr & 0xff;
		verify_cmd_v1[3] = (curr_addr >> 8) & 0xff;

		memcpy(verify_cmd_v1 + 4, file_buffer + curr_addr, pkt_length);
		//memset(verify_cmd_v1 + 4, 0xff, pkt_length);

		if (!Write(verify_cmd_v1, 64)) {
			printf("Send Verify: Fail\n");
			return 1;
		}
		if (!Read(u8Buff, 2) ) {
			printf("Read Write: Fail\n");
			return 1;
		}
		//hexdump(inbuffer, 2);

		//printf("Verify addr %d\n", curr_addr);

		curr_addr += pkt_length;
		i -= pkt_length;

		if(u8Buff[0] != 0x00)
		{
			printf("Verify failed!\n");
			return 1;
		}
	}

	return 0;
}

void v1_reset(void) {
	Write(run_cmd_v1, 4);
}

