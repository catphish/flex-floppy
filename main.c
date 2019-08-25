#include <stm32l433xx.h>
#include <stdint.h>
#include "uart.h"

int main() {
  uart_write_string("Hello!\n");

  while(1);
}
