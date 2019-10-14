#include <stdint.h>

void floppy_init();
void floppy_read_track();
void floppy_write_track();
void floppy_handle_usb_request(uint8_t ep);
