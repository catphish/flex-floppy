#include <stdint.h>

struct raw_sector {
  char data[(512+16+4+4+4)*2];
};

struct decoded_sector {
	char format;
	char track;
	char sector;
	char remaining;
	char recovery[16];
	uint32_t header_checksum;
	uint32_t data_checksum;
	char data[512];
};
