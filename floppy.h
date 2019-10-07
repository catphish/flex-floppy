#include <stdint.h>

void floppy_init();
void floppy_read_track();
void floppy_handle_usb_request(char * packet, uint8_t length);
