#include <stm32l433xx.h>
#include <stdint.h>
#include "usb.h"

extern volatile uint32_t usb_config_active;

int main() {

	while(!usb_config_active);
  while(1) {
  	// Send a text string in respinse to bulk in requests
	  if(ep_ready(0x81)) {
		  usb_write(0x81, (unsigned char *)"ABCDE\n", 6, 6);
	  }
  	// Send a text string in respinse to interrupt in requests
	  if(ep_ready(0x82)) {
		  usb_write(0x82, (unsigned char *)"12345\n", 6, 6);
	  }
	}
}
