#include <stm32l433xx.h>
#include <stdint.h>
#include "gpio.h"
#include "usb.h"
#include "floppy.h"

extern volatile uint32_t usb_config_active;

volatile uint8_t task;

int main() {
  task = 0;
  gpio_init();
  usb_init();
  floppy_init();
  //while(!usb_config_active);

  while(1) {
    // Main loop. Run task requested by USB interrupt.
    if(task == 4) {
      floppy_read_track();
      task = 0;
    } else if(task == 6) {
      floppy_write_track();
      task = 0;
    }
  }
}
