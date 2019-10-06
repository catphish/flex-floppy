#include <stm32l433xx.h>
#include <stdint.h>
#include "util.h"
#include "usb_private.h"
#include "usb.h"
#include "gpio.h"
#include "floppy.h"

uint32_t buffer_pointer = 64;
uint8_t pending_addr = 0;
uint32_t usb_config_active = 0;

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

  // Enable USB
  USB->CNTR &= ~USB_CNTR_PDWN;
  usleep(10);
  USB->CNTR &= ~USB_CNTR_FRES;
  usleep(10);
  USB->ISTR = 0;
  USB->CNTR |= USB_CNTR_RESETM;
  USB->BCDR |= USB_BCDR_DPPU;

  NVIC->ISER[2] |= (1 << (USB_IRQn - 64));
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

uint32_t ep_ready(uint32_t ep) {
  ep &= 0x7f;
  return((USB_EPR(ep) & 0x30) == 0x20);
}

uint32_t ep_rx_ready(uint32_t ep) {
  ep &= 0x7f;
  return((USB_EPR(ep) & 0x3000) == 0x2000);
}

void usb_read(uint8_t ep, char * buffer, uint32_t len) {
  ep &= 0x7f;
  while(!ep_rx_ready(ep));
  uint32_t rxBufferAddr = USBBUFTABLE->ep_desc[ep].rxBufferAddr;
  for(int n=0; n<len; n++) {
    buffer[n] = USBBUFRAW[rxBufferAddr+n];
  }
  // Clear CTR_RX and toggle NAK->VALID
  USB_EPR(ep) = (USB_EPR(ep) & 0x078f) | 0x1000;
}

void usb_write_packet(uint8_t ep, char * buffer, uint32_t len) {
  ep &= 0x7f;
  uint32_t txBufferAddr = USBBUFTABLE->ep_desc[ep].txBufferAddr;
  for(int n=0; n<len; n+=2) {
    USBBUFRAW16[(txBufferAddr+n)>>1] = buffer[n] | (buffer[n+1] << 8);
  }
  USBBUFTABLE->ep_desc[ep].txBufferCount = len;

  // Toggle NAK->VALID
  USB_EPR(ep) = (USB_EPR(ep) & 0x878f) | 0x0010;
}

void usb_write(uint8_t ep, char * buffer, uint32_t len, uint32_t limit_len) {
  if(limit_len < len) len = limit_len;
  while(len > 64) {
    while(!ep_ready(ep));
    usb_write_packet(ep, buffer, 64);
    len -= 64;
    buffer += 64;
  }
  while(!ep_ready(ep));
  usb_write_packet(ep, buffer, len);
}

void handle_setup(char * packet) {
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
  // MSC GET MAX LUN
  if(bmRequestType == 0xA1 && bRequest == 0xFE)
    usb_write(0, "\0", 1, 1);
}

void send_scsi_status(uint8_t endpoint, char * request_packet, uint8_t status) {
  char response[13];
  response[0] = 'U';
  response[1] = 'S';
  response[2] = 'B';
  response[3] = 'S';
  response[4] = request_packet[4];
  response[5] = request_packet[5];
  response[6] = request_packet[6];
  response[7] = request_packet[7];
  response[8] = 0;
  response[9] = 0;
  response[10] = 0;
  response[11] = 0;
  response[12] = status;
  usb_write(endpoint, response, 13, 13);
}

void msc_handle_usb_request(char * packet, uint8_t length) {
  uint8_t opcode = packet[15];
  uint8_t evpd = packet[16] & 1;
  if(opcode == 0x12) {
    uint16_t allocation_length = (packet[18] << 8) | packet[19];
    if(evpd) {
      send_scsi_status(0x82, packet, 1);
    } else {
      // INQUIRY
      usb_write(0x82, scsi_descriptor, 36, allocation_length);
      send_scsi_status(0x82, packet, 0);
    }
  } else if(opcode == 0x0) {
    // TEST UNIT READY
    send_scsi_status(0x82, packet, 0);
  } else if(opcode == 0x5a) {
    // MODE SENSE
    char response[] = {0,6, 0,0,0,0,0,0};
    usb_write(0x82, response, 8, 8);
    send_scsi_status(0x82, packet, 0);
  } else if(opcode == 0x25) {
    // READ CAPACITY
    char response[] = {0,0,6,224, 0,0,2,0}; // 11*160, 512
    usb_write(0x82, response, 8, 8);
    send_scsi_status(0x82, packet, 0);
  } else {
    // Unknown command
    send_scsi_status(0x82, packet, 1);
  }
}

void USB_IRQHandler() {
  if(USB->ISTR & USB_ISTR_RESET) {
    USB->ISTR &= ~USB_ISTR_RESET;
    buffer_pointer = 64;

    // Control
    usb_configure_ep(0, 1, 64);
    // Proprietary interface
    usb_configure_ep(0x01, 0, 64);
    usb_configure_ep(0x81, 0, 64);
    // Mass storage
    usb_configure_ep(0x02, 0, 64);
    usb_configure_ep(0x82, 0, 64);

    USB->BTABLE = 0;

    // Enable the peripheral
    USB->DADDR = USB_DADDR_EF;
  }

  if(USB->ISTR & USB_ISTR_CTR) {
    uint32_t ep = USB->ISTR & 0xf;
    if(USB->ISTR & 0x10) {
      // RX
      uint32_t len = USBBUFTABLE->ep_desc[ep].rxBufferCount & 0x03ff;
      char rx_buf[64];
      usb_read(ep, rx_buf, len);
      if(USB_EPR(ep) & USB_EP_SETUP) {
        handle_setup(rx_buf);
      } else if(ep == 1) {
        floppy_handle_usb_request(rx_buf, len);
      } else if(ep == 2) {
        msc_handle_usb_request(rx_buf, len);
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
