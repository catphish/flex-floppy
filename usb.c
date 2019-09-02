#include <stm32l433xx.h>
#include <stdint.h>
#include "uart.h"
#include "usb_private.h"
#include "usb.h"

uint32_t buffer_pointer = 64;
uint8_t pending_addr = 0;
uint32_t usb_config_active = 0;

// Types: 0=Bulk,1=Control,2=Iso,3=Interrupt
void usb_configure_ep(uint8_t ep, uint32_t type, uint32_t size) {
  uint8_t in = ep & 0x80;
  ep &= 0x7f;

  uint32_t old_epr = USB_EPR(ep);
  uint32_t new_epr = 0; // Always write 1 to bits 15 and 8 for no effect.
  new_epr |= (type << 9);    // Set type
  new_epr |= ep;             // Set endpoint number

  if(in || ep == 0) {
    USBBUFTABLE->ep_desc[ep].txBufferAddr = buffer_pointer;
    buffer_pointer += size;
    new_epr |= (old_epr & 0x0030) ^ 0x0020;
  }

  if(!in) {
    USBBUFTABLE->ep_desc[ep].rxBufferAddr = buffer_pointer;
    buffer_pointer += size;
    USBBUFTABLE->ep_desc[ep].rxBufferCount = (1<<15) | (1 << 10);
    new_epr |= (old_epr & 0x3000) ^ 0x3000;
  }

  USB_EPR(ep) = new_epr;
  uart_write_int(new_epr);
  uart_write_string("\n");
}

uint32_t ep_ready(uint32_t ep) {
  ep &= 0x7f;
  return((USB_EPR(ep) & 0x30) == 0x20);
}

uint32_t ep_rx_ready(uint32_t ep) {
  ep &= 0x7f;
  return((USB_EPR(ep) & 0x3000) == 0x2000);
}

void usb_read(uint8_t ep, uint8_t * buffer, uint32_t len) {
  ep &= 0x7f;
  while(!ep_rx_ready(ep));
  uint32_t rxBufferAddr = USBBUFTABLE->ep_desc[ep].rxBufferAddr;
  for(int n=0; n<len; n++) {
    buffer[n] = USBBUFRAW[rxBufferAddr+n];
  }
  // Clear CTR_RX and toggle NAK->VALID
  USB_EPR(ep) = (USB_EPR(ep) & 0x078f) | 0x1000;
}

void usb_write_packet(uint8_t ep, uint8_t * buffer, uint32_t len) {
  ep &= 0x7f;
  while(!ep_ready(ep));
  uint32_t txBufferAddr = USBBUFTABLE->ep_desc[ep].txBufferAddr;
  for(int n=0; n<len; n+=2) {
    USBBUFRAW16[(txBufferAddr+n)>>1] = buffer[n] | (buffer[n+1] << 8);
  }
  USBBUFTABLE->ep_desc[ep].txBufferCount = len;

  // Toggle NAK->VALID
  USB_EPR(ep) = (USB_EPR(ep) & 0x878f) | 0x0010;
}

void usb_write(uint8_t ep, uint8_t * buffer, uint32_t len, uint32_t limit_len) {
  if(limit_len < len) len = limit_len;
  while(len > 64) {
    usb_write_packet(ep, buffer, 64);
    len -= 64;
    buffer += 64;
  }
  usb_write_packet(ep, buffer, len);
}

void handle_setup(uint8_t * packet) {
  uint8_t bmRequestType = packet[0];
  uint8_t bRequest      = packet[1];
  // Descriptor request
  if(bmRequestType == 0x80 && bRequest == 0x06) {
    uint8_t descriptor_type = packet[3];
    uint8_t descriptor_idx  = packet[2];

    if(descriptor_type == 1 && descriptor_idx == 0) {
      usb_write(0, device_descriptor, 18, packet[6]);
    }
    if(descriptor_type == 2 && descriptor_idx == 0) {
      usb_write(0, config_descriptor, config_descriptor[2], packet[6]);
    }
  }
  // Set address
  if(bmRequestType == 0x00 && bRequest == 0x05) {
    pending_addr = packet[2];
    usb_write(0,0,0,0);
  }
  // Set configuration
  if(bmRequestType == 0x00 && bRequest == 0x09) {
    usb_write(0,0,0,0);
    usb_config_active = 1;
  }
}

void USB_IRQHandler() {
  if(USB->ISTR & USB_ISTR_RESET) {
    USB->ISTR &= ~USB_ISTR_RESET;
    buffer_pointer = 64;

    usb_configure_ep(0, 1, 64);

    usb_configure_ep(0x81, 3, 64);
    usb_configure_ep(0x02, 0, 64);
    usb_configure_ep(0x82, 0, 64);

    USB->BTABLE = 0;

    // Enable the peripheral
    USB->DADDR = USB_DADDR_EF;
    uart_write_string("RST\n");
  }

  if(USB->ISTR & USB_ISTR_CTR) {
    uint32_t ep = USB->ISTR & 0xf;
    if(USB->ISTR & 0x10) {
      // RX
      uint32_t len = USBBUFTABLE->ep_desc[ep].rxBufferCount & 0x03ff;
      uint8_t rx_buf[64];
      usb_read(0, rx_buf, len);
      if(USB_EPR(ep) & USB_EP_SETUP) {
        handle_setup(rx_buf);
      }
    } else {
      // TX
      USB_EPR(ep) &= 0x870f;

      if(pending_addr) {
        USB->DADDR = USB_DADDR_EF | pending_addr;
        pending_addr = 0;
      }
    }
  }

}
