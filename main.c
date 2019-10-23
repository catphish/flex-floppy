#include <stm32l433xx.h>
#include <stdint.h>
#include "gpio.h"
#include "usb.h"
#include "floppy.h"

int main() {
  gpio_init();
  usb_init();
  floppy_init();

  while(1) {
    usb_main_loop();
		floppy_main_loop();
  }
}
