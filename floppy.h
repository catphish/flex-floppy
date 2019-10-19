#include <stdint.h>

#define MEMORY_SIZE 8192

void floppy_init();
void handle_ep1();

void floppy_enable();
void floppy_disable();
void floppy_write_enable();
void floppy_write_disable();

void disable_timer();
void floppy_start_read();

void track_zero();
void track_seek(uint8_t target);
void terminate_data();
