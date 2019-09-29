#include <stdint.h>

void floppy_init();
void floppy_read_track();
void floppy_handle_usb_request(uint8_t * packet, uint8_t length);

struct raw_sector {
  char data[(512+16+4+4+4)*2];
};

struct decoded_sector {
	unsigned char format;
	unsigned char track;
	unsigned char sector;
	unsigned char remaining;
	unsigned char recovery[16];
	uint32_t header_checksum;
	uint32_t data_checksum;
	unsigned char data[512];
};