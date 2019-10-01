#include <stdint.h>

void floppy_init();
void floppy_read_track();
void floppy_handle_usb_request(char * packet, uint8_t length);

struct sector {
	char raw[1088];
	unsigned char format;
	unsigned char track;
	unsigned char sector;
	unsigned char remaining;
	uint32_t header_checksum;
	uint32_t data_checksum;
};