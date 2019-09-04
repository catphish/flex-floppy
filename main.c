#include <stm32l433xx.h>
#include <stdint.h>
#include "gpio.h"
#include "usb.h"
#include "floppy.h"

extern volatile uint32_t usb_config_active;

int main() {
	gpio_init();
  usb_init();
	while(!usb_config_active);
  floppy_init();
	floppy_read_track();
	while(1);
}
