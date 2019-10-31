#include <stm32l433xx.h>
#include <stdint.h>
#include "util.h"
#include "usb_private.h"
#include "usb.h"
#include "gpio.h"
#include "floppy.h"

uint8_t pending_addr = 0;
uint8_t rx_rdy;

void usb_init() {
  gpio_port_mode(GPIOA, 11, 2, 10, 0, 0); // A11 AF10
  gpio_port_mode(GPIOA, 12, 2, 10, 0, 0); // A12 AF10

  // Enable Power control clock
  RCC->APB1ENR1 |= RCC_APB1ENR1_PWREN;
  // Enable USB clock
  RCC->APB1ENR1 |= RCC_APB1ENR1_USBFSEN;
  // CRS Clock eneble
  RCC->APB1ENR1 |= RCC_APB1ENR1_CRSEN;

  // Enable HSI48
  RCC->CRRCR |= RCC_CRRCR_HSI48ON;
  while ((RCC->CRRCR & RCC_CRRCR_HSI48RDY) != RCC_CRRCR_HSI48RDY);

  // Enable USB Power
  PWR->CR2 |= (1<<10);
  usleep(10);

  // Enable USB and reset
  USB->CNTR = USB_CNTR_FRES;
  usleep(10);
  // Deassert reset
  USB->CNTR = 0;
  usleep(10);
  // Activate DP pullup
  USB->BCDR |= USB_BCDR_DPPU;
  rx_rdy = 0;
}

uint8_t usb_rx_ready(uint8_t ep) {
  // Use a dummy packet size to mark buffers as empty
  if(USB_EPR(ep) & 0x40) {
    return(USBBUFTABLE->ep_desc[ep].rxBufferCount != ((1<<15) | (1 << 10) | 0xff));
  } else {
    return(USBBUFTABLE->ep_desc[ep].txBufferCount != ((1<<15) | (1 << 10) | 0xff));
  }
}

uint8_t usb_tx_ready(uint8_t ep) {
  ep &= 0xf;
  uint32_t epr = USB_EPR(ep);
  if((epr & 0x30) == 0x30) {
    // READY, transmit unless SW_BUF == DTOG
    if((epr & 0x4000) != ((epr & 0x40) << 8)) {
      return(1); // Free buffer available
    } else {
      return(0); // All buffers full
    }
  } else {
    // NAK, always transmit
    return(1);
  }
}

uint32_t buffer_pointer;
// Types: 0=Bulk,1=Control,2=Iso,3=Interrupt, 0x10=Bulk(dbl_buf)
void usb_configure_ep(uint8_t ep, uint32_t type) {
  uint8_t in = ep & 0x80;
  ep &= 0x7f;

  int dblbuf = (type >> 4) & 1;
  uint32_t old_epr = USB_EPR(ep);
  uint32_t new_epr = 0; // Always write 1 to bits 15 and 8 for no effect.
  new_epr |= ((type & 3) << 9); // Set type
  new_epr |= (dblbuf << 8);     // Set kind
  new_epr |= ep;                // Set endpoint number

  if(in || ep == 0) {
    USBBUFTABLE->ep_desc[ep].txBufferAddr = buffer_pointer;
    USBBUFTABLE->ep_desc[ep].txBufferCount = 0;
    buffer_pointer += 64;
    if(dblbuf) {
      // Use RX buffer as additional TX buffer
      USBBUFTABLE->ep_desc[ep].rxBufferAddr = buffer_pointer;
      USBBUFTABLE->ep_desc[ep].rxBufferCount = 0;
      buffer_pointer += 64;
      new_epr |= (old_epr & 0x4040) ^ 0x40; // DTOG=1, SW_BUF=0
    }
    new_epr |= (old_epr & 0x0030) ^ 0x0020; // NAK
  }

  if(!in) {
    USBBUFTABLE->ep_desc[ep].rxBufferAddr = buffer_pointer;
    USBBUFTABLE->ep_desc[ep].rxBufferCount = (1<<15) | (1 << 10) | 0xff;
    buffer_pointer += 64;

    if(dblbuf) {
      // Use TX buffer as additional RX buffer
      USBBUFTABLE->ep_desc[ep].txBufferAddr = buffer_pointer;
      USBBUFTABLE->ep_desc[ep].txBufferCount = (1<<15) | (1 << 10) | 0xff;
      buffer_pointer += 64;
      new_epr |= (old_epr & 0x4040); // DTOG=0, SW_BUF=0
    }
    new_epr |= (old_epr & 0x3000) ^ 0x3000; // Ready
  }

  USB_EPR(ep) = new_epr;
}

void usb_read(uint8_t ep, volatile char * buffer) {
  ep &= 0x7f;
  USB_EPR(ep) &= 0x078f;
  uint32_t rxBufferAddr = USBBUFTABLE->ep_desc[ep].rxBufferAddr;
  if(buffer) {
    for(int n=0; n<64; n+=2) {
      *(uint16_t *)(buffer + n) = *(uint16_t *)(USBBUFRAW+rxBufferAddr+n);
    }
  }
  // RX NAK->VALID
  USB_EPR(ep) = (USB_EPR(ep) & 0xb78f) ^ 0x3000;
}

void usb_read_dbl(uint8_t ep, volatile char * buffer) {
  ep &= 0x7f;
  uint32_t epr = USB_EPR(ep);
  uint32_t rxBufferAddr;

  // Use a dummy packet size to mark buffers as empty
  if(epr & 0x40) {
    rxBufferAddr = USBBUFTABLE->ep_desc[ep].rxBufferAddr;
    USBBUFTABLE->ep_desc[ep].rxBufferCount = (1<<15) | (1 << 10) | 0xff;
  } else {
    rxBufferAddr = USBBUFTABLE->ep_desc[ep].txBufferAddr;
    USBBUFTABLE->ep_desc[ep].txBufferCount = (1<<15) | (1 << 10) | 0xff;
  }

  if(buffer) {
    for(int n=0; n<64; n+=2) {
      *(uint16_t *)(buffer + n) = *(uint16_t *)(USBBUFRAW+rxBufferAddr+n);
    }
  }
  // Toggle SW_BUF
  epr &= 0x070f;
  epr |= 0x80c0;
  USB_EPR(ep) = epr;
}

void usb_write(uint8_t ep, volatile char * buffer, uint32_t len) {
  ep &= 0x7f;
  uint32_t txBufferAddr = USBBUFTABLE->ep_desc[ep].txBufferAddr;
  for(int n=0; n<len; n+=2) {
    *(volatile uint16_t *)(USBBUFRAW+txBufferAddr+n) = *(uint16_t *)(buffer + n);
  }
  USBBUFTABLE->ep_desc[ep].txBufferCount = len;

  // TX NAK->VALID
  USB_EPR(ep) = (USB_EPR(ep) & 0x87bf) ^ 0x30;
}

void usb_write_dbl(uint8_t ep, volatile char * buffer, uint32_t len) {
  ep &= 0x7f;

  uint32_t txBufferAddr;

  if(USB_EPR(ep) & 0x4000) {
    txBufferAddr = USBBUFTABLE->ep_desc[ep].rxBufferAddr;
  } else {
    txBufferAddr = USBBUFTABLE->ep_desc[ep].txBufferAddr;
  }

  for(int n=0; n<len; n+=2) {
    *(volatile uint16_t *)(USBBUFRAW+txBufferAddr+n) = *(uint16_t *)(buffer + n);
  }

  if(USB_EPR(ep) & 0x4000) {
    USBBUFTABLE->ep_desc[ep].rxBufferCount = len;
  } else {
    USBBUFTABLE->ep_desc[ep].txBufferCount = len;
  }

  // Toggle TX SW_BUF
  USB_EPR(ep) = (USB_EPR(ep) & 0x878f) | 0x4000;

  if((USB_EPR(ep) & 0x0030) == 0x20) // TX is NAK
    if((USB_EPR(ep) & 0x4000) == ((USB_EPR(ep) & 0x40) << 8)) // Both buffers full
      USB_EPR(ep) = (USB_EPR(ep) & 0x87bf) ^ 0x30; // Set TX VALID
}

void usb_reset() {
  USB->ISTR &= ~USB_ISTR_RESET;
  buffer_pointer = 64;

  usb_configure_ep(0, 1);
  usb_configure_ep(0x01, 0x10);
  usb_configure_ep(0x82, 0x10);

  USB->BTABLE = 0;

  // Enable the peripheral
  USB->DADDR = USB_DADDR_EF;
}

void usb_handle_ep0() {
  char packet[64];
  usb_read(0, packet);

  uint8_t bmRequestType = packet[0];
  uint8_t bRequest      = packet[1];

  // Descriptor request
  if(bmRequestType == 0x80 && bRequest == 0x06) {
    uint8_t descriptor_type = packet[3];
    uint8_t descriptor_idx  = packet[2];

    if(descriptor_type == 1 && descriptor_idx == 0) {
      uint32_t len = 18;
      if(len > packet[6]) len = packet[6];
      usb_write(0, device_descriptor, len);
    }
    if(descriptor_type == 2 && descriptor_idx == 0) {
      uint32_t len = config_descriptor[2];
      if(len > packet[6]) len = packet[6];
      usb_write(0, config_descriptor, len);
    }
  }
  // Set address
  if(bmRequestType == 0x00 && bRequest == 0x05) {
    pending_addr = packet[2];
    usb_write(0,0,0);
  }
  // Set configuration
  if(bmRequestType == 0x00 && bRequest == 0x09) {
    usb_write(0,0,0);
  }

  if(bmRequestType == 0x41)
    floppy_handle_ep0(packet);

}

void usb_main_loop() {
  // USB reset
  if(USB->ISTR & USB_ISTR_RESET) {
    usb_reset();
  }

  if(USB_EPR(0) & (1<<15)) {
    // EP0 RX
    USB_EPR(0) &= 0x078f;
    usb_handle_ep0();
  }

  if(USB_EPR(0) & (1<<7)) {
    // EP0 TX
    if(pending_addr) {
      USB->DADDR = USB_DADDR_EF | pending_addr;
      pending_addr = 0;
    }
    // Clear pending state
    USB_EPR(0) &= 0x870f;
  }
}
