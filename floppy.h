#include <stdint.h>

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

void floppy_start_read();
void floppy_start_write();
void floppy_really_start_write();

void floppy_main_loop();
void floppy_handle_ep0(char * packet);
void floppy_handle_ep1();
