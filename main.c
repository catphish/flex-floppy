#include <stm32l433xx.h>
#include <stdint.h>
#include "gpio.h"
#include "usb.h"
#include "floppy.h"

extern volatile char data[MEMORY_SIZE];
extern volatile uint16_t data_in_ptr;
extern volatile uint16_t data_out_ptr;
extern volatile uint8_t overflow;

volatile uint8_t task;
volatile uint8_t target_track;
volatile uint32_t read_length;

int main() {
  task = 0;
  gpio_init();
  usb_init();
  floppy_init();

  uint8_t data_terminated;

  while(1) {
    // Main loop. Process USB Packets.

    // USB reset occurred. Reinitialize everything.
    if(USB->ISTR & USB_ISTR_RESET) {
      usb_reset();
      task = 0;
    }

    // USB packet transfer complete
    if(USB->ISTR & USB_ISTR_CTR) {
      uint32_t ep = USB->ISTR & 0xf;
      if(USB->ISTR & 0x10) {
        // RX
        if(ep == 0) {
          handle_ep0();
        } else if(ep == 1) {
          //handle_ep1();
        } else {
          // Discard
          USB_EPR(ep) = (USB_EPR(ep) & 0x378f) ^ 0x3000;
        }
      } else {
        usb_handle_tx_complete();
        // Clear pending state
        USB_EPR(ep) &= 0x870f;
      }
    }

    if(task == 5) { // Reading data
      // Stream data to USB
      if(ep_tx_ready(0x81)) {
        if((data_in_ptr >= (data_out_ptr + 64)) || (data_in_ptr < data_out_ptr)) {
          usb_write(0x81, data + data_out_ptr, 64);
          data_out_ptr = (data_out_ptr + 64) % MEMORY_SIZE;
        }
      }
      // Stop reading after fixed interval
      if(TIM2->CNT >= read_length) {
        task = 6;
        data_terminated = 0;
        disable_timer();
      }
    }

    if(task == 6) {
      // Finish streaming data to USB
      if(ep_tx_ready(0x81)) {
        if((data_in_ptr >= (data_out_ptr + 64)) || (data_in_ptr < data_out_ptr)) {
          usb_write(0x81, data + data_out_ptr, 64);
          data_out_ptr = (data_out_ptr + 64) % MEMORY_SIZE;
        } else {
          // No more 64 byte blocks. Termiate the stream or send final block.
          if(data_terminated) {
            if((data_in_ptr != data_out_ptr)) {
              // Always write a full packet. This will include trailing junk.
              usb_write(0x81, data + data_out_ptr, 64);
            }
            // Terminate the bulk transfer with an empty packet.
            usb_write(0x81, 0, 0);
            task = 0;
          } else {
            terminate_data();
            data_terminated = 1;
          }
        }
      }
    }
  }
}
