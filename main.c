#include <stm32l433xx.h>
#include <stdint.h>
#include "uart.h"
#include "usb.h"

extern volatile uint32_t usb_config_active;

int main() {
  uart_write_string("Hello!\n");

	while(!usb_config_active);
  uart_write_string("Go!\n");
  while(1) {
  	// Send a text string in respinse to bulk in requests
	  if(ep_ready(0x82)) {
		  usb_write(0x82, (unsigned char *)"ABCDE\n", 6, 6);
	  }
	  // Send empty responses to interrupt in requests
	  if(ep_ready(0x81)) {
		  usb_write(0x81, (unsigned char *)0,0,0);
		}
  }
}
