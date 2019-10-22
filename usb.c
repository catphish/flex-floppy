#include <stm32l433xx.h>
#include <stdint.h>
#include "util.h"
#include "usb_private.h"
#include "usb.h"
#include "gpio.h"
#include "floppy.h"

extern volatile uint8_t task;
extern volatile uint8_t target_track;
extern volatile uint32_t read_length;

uint32_t buffer_pointer = 64;
uint8_t pending_addr = 0;

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
}

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
}

uint32_t ep_tx_ready(uint32_t ep) {
  ep &= 0x7f;
  return((USB_EPR(ep) & 0x30) == 0x20);
}

uint32_t ep_rx_ready(uint32_t ep) {
  ep &= 0x7f;
  return((USB_EPR(ep) & 0x3000) == 0x2000);
}

void usb_read(uint8_t ep, volatile char * buffer) {
  ep &= 0x7f;
  while(!ep_rx_ready(ep));
  if(buffer) {
    uint32_t rxBufferAddr = USBBUFTABLE->ep_desc[ep].rxBufferAddr;
    uint32_t len = USBBUFTABLE->ep_desc[ep].rxBufferCount & 0x03ff;
    for(int n=0; n<len; n+=2) {
      *(uint16_t *)(buffer + n) = *(uint16_t *)(USBBUFRAW+rxBufferAddr+n);
    }
  }
  // Clear CTR_RX and toggle NAK->VALID
  USB_EPR(ep) = (USB_EPR(ep) & 0x378f) ^ 0x3000;
}

void usb_write(uint8_t ep, volatile char * buffer, uint32_t len) {
  ep &= 0x7f;
  while(!ep_tx_ready(ep));
  uint32_t txBufferAddr = USBBUFTABLE->ep_desc[ep].txBufferAddr;
  for(int n=0; n<len; n+=2) {
    *(volatile uint16_t *)(USBBUFRAW+txBufferAddr+n) = *(uint16_t *)(buffer + n);
  }
  USBBUFTABLE->ep_desc[ep].txBufferCount = len;

  // Toggle NAK->VALID
  USB_EPR(ep) = (USB_EPR(ep) & 0x87bf) ^ 0x0030;
}

void usb_reset() {
  USB->ISTR &= ~USB_ISTR_RESET;
  buffer_pointer = 64;

  usb_configure_ep(0, 1, 64);
  usb_configure_ep(0x01, 0, 64);
  usb_configure_ep(0x81, 0, 64);

  USB->BTABLE = 0;

  // Enable the peripheral
  USB->DADDR = USB_DADDR_EF;
}

void usb_handle_tx_complete() {
  // TX
  if(pending_addr) {
    USB->DADDR = USB_DADDR_EF | pending_addr;
    pending_addr = 0;
  }
}

void handle_ep0() {
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

  // Enable Drive
  if(bmRequestType == 0x41 && bRequest == 0x01) {
    task = 0;
    floppy_enable();
    usb_write(0,0,0);
  }
  // Disble Drive
  if(bmRequestType == 0x41 && bRequest == 0x02) {
    task = 0;
    floppy_disable();
    usb_write(0,0,0);
  }
  // Zero Head
  if(bmRequestType == 0x41 && bRequest == 0x11) {
    // This is a slow blocking operation!
    task = 0;
    track_zero();
    usb_write(0,0,0);
  }
  // Seek Head
  if(bmRequestType == 0x41 && bRequest == 0x12) {
    // This is a slow blocking operation!
    task = 0;
    track_seek(packet[2]);
    usb_write(0,0,0);
  }
  // Read Track
  if(bmRequestType == 0x41 && bRequest == 0x21) {
    read_length  = (uint32_t)packet[2] * 1000000;
    task = 5;
    floppy_start_read();
    usb_write(0,0,0);
  }
  // Write Track
  if(bmRequestType == 0x41 && bRequest == 0x31) {
    task = 7;
    floppy_prepare_write();
    usb_write(0,0,0);
  }
}
