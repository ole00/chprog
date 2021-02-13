#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <libusb-1.0/libusb.h>


uint8_t chipID;
uint8_t u8Buff[64];

libusb_device_handle *h;


int v2_detect(void);
int v2_write(uint8_t* pReadBuff, int fileSize);
int v2_verify(uint8_t* pReadBuff, int fileSize);
void v2_reset(void);

int v1_detect(void);
int v1_write(uint8_t* pReadBuff, int fileSize);
int v1_verify(uint8_t* pReadBuff, int fileSize);
void v1_reset(void);

uint32_t Write(uint8_t *p8Buff, uint8_t u8Length)
{
	int len;
	if (libusb_bulk_transfer(h, 0x02, (unsigned char*)p8Buff, u8Length, &len, 5000) != 0) {
		return 0;
	} else {
		return 1;
	}
	return 0;
}
uint32_t Read(uint8_t *p8Buff, uint8_t u8Length)
{
	int len;
	if (libusb_bulk_transfer(h, 0x82, (unsigned char*)p8Buff, u8Length, &len, 5000) != 0) {
		return 0;
	} else {
		return 1;
	}
	return 0;
}

uint8_t* LoadFile(int maxSize, char* fname, int* fsize)
{
    FILE *f;
    uint8_t* pReadBuff = (uint8_t*)malloc(maxSize);
    int r;
    if (pReadBuff == NULL) {
        return 0;
    }
    memset(pReadBuff, 0xFF, maxSize);
  
    f = fopen(fname, "rb");
    if (f == NULL) {
        return 0;
    }
    r = fread(pReadBuff, 1, maxSize, f); 
    if (r < 1) {
    	fclose(f);
    	return 0;
    }
    fclose(f);
    *fsize = r;
    return pReadBuff;
}

static int protocol_detect(void) {
	/* detect protocol: v1 returns 0xFF, v2 returnx 0xa2 */
	static char detect_prot_cmd[] = {0xa2, 0x1, 0x00};
	char detect_result[6] = {0};

	if (!Write(detect_prot_cmd, sizeof(detect_prot_cmd))) {
		printf("Send Init: Fail\n");
		return -1;
	}
	
	if (!Read(detect_result, sizeof(detect_result))) {
		printf("Read Init: Fail\n");
		return -1;
	}
	return detect_result[0] & 0xFF;
}

static int get_max_flash_size(int id) {
	switch (id) {
		case 0x59: return 61440;
		case 0x52:
		case 0x54: return 14336;
		default: return 10240; 
	}
}

int main(int argc, char const *argv[])
{
	int ret = 1;
	int maxSize;
	uint8_t* pReadBuff;
	int fileSize = 0;
	int (*ch_detect)(void);
	int (*ch_write)(uint8_t* pReadBuff, int fileSize);
	int (*ch_verify)(uint8_t* pReadBuff, int fileSize);
	void (*ch_reset)(void);

	printf("CH55x Programmer by VNPro & rgwan & olin\n");
	if (argc != 2) {
		printf("------------------------------------------------------------------\n");
		printf("usage: chprog flash_file.bin\n");
		printf("------------------------------------------------------------------\n");
	}
	if (argc == 2) {
		    /* load flash file */
		    pReadBuff = LoadFile(64 * 1024, (char*)argv[1], &fileSize); 
			if (!pReadBuff) {
				printf("Error: failed to read file (%s)\n", argv[1]);
		        return 1;
			} else {
			    printf ("File read (%s): %i bytes\n", argv[1], fileSize);
			}
	}

	libusb_init(NULL);
	libusb_set_debug(NULL, 3);
	
	h = libusb_open_device_with_vid_pid(NULL, 0x4348, 0x55e0);

	if (h == NULL) {
		printf("Error: CH55x USB device not found.\n");
		return 1;
	}
	
	if (libusb_claim_interface(h, 0)) {
		printf("Failed to claim interface 0\n");
		return 1;
	}


	ch_detect = v2_detect;
	ch_write = v2_write;
	ch_verify = v2_verify;
	ch_reset = v2_reset;

	// v2 prot. returns 0xa2, v1 prot. returns 0xff	
	if (protocol_detect() != 0xa2) {
		ch_detect = v1_detect;
		ch_write = v1_write;
		ch_verify = v1_verify;
		ch_reset = v1_reset;
		printf("Using v1 protocol\n");
	} else {
		printf("Using v2 protocol\n");
	}
	
	// detect chip and print bootloader info 
	if (ch_detect()) {
		goto finish;
	}
	
	// no file 
	if (fileSize < 1) {
		goto finish;
	}
	
	maxSize = get_max_flash_size(chipID);
	if (fileSize > maxSize) {
		printf("FW size is too big to fit the flash: %i max: %i\n", fileSize, maxSize);
		goto finish;
	}

	// write the firmware to the chip
	if (ch_write(pReadBuff, fileSize)) {
		goto finish;
	}
	
	// verify written data
	if (ch_verify(pReadBuff, fileSize)) {
		goto finish;
	}

	printf("Write & verify complete. All seems OK.\n");
	
	ch_reset();
	
	ret = 0;
	
finish:
	if (h)
	{
		libusb_release_interface(h, 0);
		libusb_close(h);
	}
	libusb_exit(NULL);

	return ret;
}
