#include <stm32l433xx.h>
void usleep(unsigned int delay) {
  SysTick->LOAD = 0x00FFFFFF;
  SysTick->CTRL = 5;
  SysTick->VAL = 0;
  while(0x00FFFFFF - SysTick->VAL < delay * 80);
}