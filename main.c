#include <stm32l433xx.h>
#include <stdint.h>
#include "gpio.h"
#include "usb.h"
#include "floppy.h"

int main() {
  gpio_init();
  floppy_init();
  usb_init();

  while(1) {
    usb_main_loop();
    if(TIM2->CNT > 160000000) {
      // 2-second timeout
      floppy_disable();
    }
  }
}
