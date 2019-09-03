#include <stm32l433xx.h>
#include <stdint.h>
#include "gpio.h"

/*
	A1  = READY
  A2  = HEADSET
  A8  = INDEX
  A9  = DENSITY

  B0  = PROTECT
  B1  = TRACK0
  B2  = WENABLE
  B10 = RDATA
  B11 = WDATA
  B12 = STEP
  B13 = DIR
  B14 = MOTOR
  B15 = ENABLE
*/

void floppy_init() {
  gpio_port_mode(GPIOB, 14, 1, 0, 0, 1);
  gpio_port_mode(GPIOB, 15, 1, 0, 0, 1);
}
