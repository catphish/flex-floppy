struct sector {
	char raw[1088];
	char assembled[512];
	unsigned char format;
	unsigned char track;
	unsigned char sector;
	unsigned char remaining;
	uint32_t header_checksum;
	uint32_t data_checksum;
};

void floppy_init();
void floppy_track_minus();
void floppy_track_plus();
void floppy_set_side(uint8_t side);
void floppy_track_zero();
void floppy_track_seek(uint8_t target);
void floppy_enable();
void floppy_disable();
void floppy_write_enable();
void floppy_write_disable();
void start_timer();
char * floppy_read_sector(uint16_t block);
