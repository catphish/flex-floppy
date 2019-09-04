#include <stm32l433xx.h>
void usleep(unsigned int delay) {
  SysTick->LOAD = 0x00FFFFFF;
  SysTick->VAL = 0;
  SysTick->CTRL = 5;
  while(0x00FFFFFF - SysTick->VAL < delay * 80);
}