#include <stm32l433xx.h>
#include <stdint.h>
#include "uart.h"

#define USB_EPR(n) (*(volatile uint16_t *)(USB_BASE + 4 * n))
#define USBBUFTABLE ((volatile struct USBBufTable *)0x40006C00)
#define USBBUFRAW ((volatile uint8_t *)0x40006C00)
#define USBBUFRAW16 ((volatile uint16_t *)0x40006C00)

uint8_t device_descriptor[] = {18, 1, 1,1, 0, 0, 0, 64, 0x09,0x12, 0x01,0x00, 1,0, 0,0,0, 1 };

struct USBBufTable {
  struct USBBufDesc {
    uint16_t txBufferAddr ;
    uint16_t txBufferCount ;
    uint16_t rxBufferAddr ;
    uint16_t rxBufferCount ;
  }
  ep_desc[8];
};

uint32_t buffer_pointer = 64;
uint8_t pending_addr = 0;

// Types: 0=Bulk,1=Control,2=Iso,3=Interrupt
void usb_configure_ep(uint8_t ep, uint8_t type, uint32_t size) {
  uart_write_string("USB: Setting up EP!\n");

  uint8_t in = ep & 0x80;
  ep &= 0x7f;

  uint32_t old_epr = USB_EPR(ep);
  uint32_t new_epr = 0x8080; // Always write 1 to bits 15 and 8 for no effect.
  new_epr |= (type << 9);    // Set type
  new_epr |= ep;             // Set endpoint number

  if(in || ep == 0) {
    USBBUFTABLE->ep_desc[ep].txBufferAddr = buffer_pointer;
    buffer_pointer += size;
    new_epr |= (old_epr & 0x0070) ^ 0x0020;
  }

  if(!in) {
    USBBUFTABLE->ep_desc[ep].rxBufferAddr = buffer_pointer;
    buffer_pointer += size;
    USBBUFTABLE->ep_desc[ep].rxBufferCount = (1<<15) | (1 << 10);
    new_epr |= (old_epr & 0x7000) ^ 0x3000;
  }

  USB_EPR(ep) = new_epr;
}

void usb_read(uint8_t ep, uint8_t * buffer, uint32_t len) {
  uart_write_string(" USB: Read data: ");
  uint32_t rxBufferAddr = USBBUFTABLE->ep_desc[ep].rxBufferAddr;
  for(int n=0; n<len; n++) {
    buffer[n] = USBBUFRAW[rxBufferAddr+n];
    uart_write_int(buffer[n]);
    uart_write_string(" ");
  }
  uart_write_string("\n");
}

void usb_write(uint8_t ep, uint8_t * buffer, uint32_t len) {
  uart_write_string(" USB: Write data: ");
  uint32_t txBufferAddr = USBBUFTABLE->ep_desc[ep].txBufferAddr;
  for(int n=0; n<len; n+=2) {
    USBBUFRAW16[(txBufferAddr+n)>>1] = buffer[n] | (buffer[n+1] << 8);
    uart_write_int(buffer[n]);
    uart_write_string(" ");
    uart_write_int(buffer[n+1]);
    uart_write_string(" ");
  }
  uart_write_string("\n");
  USBBUFTABLE->ep_desc[ep].txBufferCount = len;

  // Clear CTR_TX and toggle NAK->VALID
  USB_EPR(ep) = (USB_EPR(ep) & 0x870f) | 0x0010;
}

void handle_setup(uint8_t * packet) {
  uint8_t bmRequestType = packet[0];
  uint8_t bRequest      = packet[1];
  if(bmRequestType == 0x80 && bRequest == 0x06) {
    uart_write_string(" USB: GET_DESCRIPTOR\n");
    uint8_t descriptor_type = packet[3];
    uint8_t descriptor_idx  = packet[2];
    if(descriptor_type == 1 && descriptor_idx == 0) {
      usb_write(0, device_descriptor, 18);
    }
  }
  if(bmRequestType == 0x00 && bRequest == 0x05) {
    uart_write_string(" USB: SET_ADDRESS\n");
    pending_addr = packet[2];
    usb_write(0,0,0);
  }
}

void USB_IRQHandler() {
  if(USB->ISTR & USB_ISTR_RESET) {
    USB->ISTR &= ~USB_ISTR_RESET;
    uart_write_string("USB: Reset!\n");
    // Configure endpoint 0 for control
    usb_configure_ep(0, 1, 64);
        
    USB->BTABLE = 0;

    // Enable the peripheral
    USB->DADDR = USB_DADDR_EF;

  }
  
  if(USB->ISTR & USB_ISTR_CTR) {
    uart_write_string("USB: Transfer complete\n");
    uint32_t ep = USB->ISTR & 0xf;
    if(USB->ISTR & 0x10) {
      // RX
      uart_write_string(" USB: Packet received\n");
      if(USB_EPR(ep) & USB_EP_SETUP) {
        uart_write_string(" USB: Processing setup packet\n");
        uint8_t setup_buf[8];
        usb_read(0, setup_buf, 8);
        handle_setup(setup_buf);
      } else {
        uart_write_string(" USB: Processing data packet\n");
        uint32_t len = USBBUFTABLE->ep_desc[ep].rxBufferCount & 0x03ff;
        uint8_t data_buf[64];
        usb_read(ep, data_buf, len);
      }
      // Clear CTR_RX and toggle NAK->VALID
      USB_EPR(ep) = (USB_EPR(ep) & 0x078f) | 0x1000;
    } else {
      // TX
      uart_write_string(" USB: Packet transmitted\n");
      if(pending_addr) {
        uart_write_string(" USB: Updated address\n");
        USB->DADDR = USB_DADDR_EF | pending_addr;
        pending_addr = 0;
      }
      USB_EPR(ep) = (USB_EPR(ep) & 0x870f);
    }
  }

  if(USB->ISTR) {
    // uart_write_string("USB: Unknown interrupt: ");
    // uart_write_int(USB->ISTR);
    // uart_write_nl();
    USB->ISTR = 0;
  }
}
