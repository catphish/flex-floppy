#include <stm32l433xx.h>
#include <stdint.h>
#include "gpio.h"
#include "usb.h"

extern volatile uint32_t usb_config_active;

int main() {
  gpio_init();
  usb_init();
  char data[64];

  while(!usb_config_active);

  while(1) {
    usb_write_dbl(0x82, data, 64);
  }
}
