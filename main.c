#include <stm32l433xx.h>
#include <stdint.h>
#include "gpio.h"
#include "usb.h"

int main() {
  gpio_init();
  usb_init();

  while(1) {
    usb_main_loop();
  }
}
