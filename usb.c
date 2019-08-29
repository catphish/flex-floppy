#include <stm32l433xx.h>
#include <stdint.h>
#include "uart.h"

#define USB_EPR(n) (*(volatile uint16_t *)(USB_BASE + 4 * n))
#define USBBUFTABLE ((volatile struct USBBufTable *)0x40006C00)
#define USBBUFRAW ((volatile uint8_t *)0x40006C00)
#define USBBUFRAW16 ((volatile uint16_t *)0x40006C00)

uint8_t device_descriptor[] = {18, 1, 0x10,0x01, 0, 0, 0, 64, 0x09,0x12, 0x01,0x00, 1,0, 0,0,0, 1 };
uint8_t config_descriptor[] = {
/*Configuation Descriptor*/
        0x09,   /* bLength: Configuation Descriptor size */
        2,      /* bDescriptorType: Configuration */
        67,     /* wTotalLength:no of returned bytes */
        0x00,
        0x02,   /* bNumInterfaces: 2 interface */
        0x01,   /* bConfigurationValue: Configuration value */
        0x00,   /* iConfiguration: Index of string descriptor describing the configuration */
        0x80,   /* bmAttributes: bus powered */
        50,     /* MaxPower 100 mA */
/*Interface Descriptor*/
        0x09,   /* bLength: Interface Descriptor size */
        4,      /* bDescriptorType: Interface */
                        /* Interface descriptor type */
        0x00,   /* bInterfaceNumber: Number of Interface */
        0x00,   /* bAlternateSetting: Alternate setting */
        0x01,   /* bNumEndpoints: One endpoints used */
        0x02,   /* bInterfaceClass: Communication Interface Class */
        0x02,   /* bInterfaceSubClass: Abstract Control Model */
        0x01,   /* bInterfaceProtocol: Common AT commands */
        0x00,   /* iInterface: */
/*Header Functional Descriptor*/
        0x05,   /* bLength: Endpoint Descriptor size */
        0x24,   /* bDescriptorType: CS_INTERFACE */
        0x00,   /* bDescriptorSubtype: Header Func Desc */
        0x10,   /* bcdCDC: spec release number */
        0x01,
/*Call Managment Functional Descriptor*/
        0x05,   /* bFunctionLength */
        0x24,   /* bDescriptorType: CS_INTERFACE */
        0x01,   /* bDescriptorSubtype: Call Management Func Desc */
        0x00,   /* bmCapabilities: D0+D1 */
        0x01,   /* bDataInterface: 1 */
/*ACM Functional Descriptor*/
        0x04,   /* bFunctionLength */
        0x24,   /* bDescriptorType: CS_INTERFACE */
        0x02,   /* bDescriptorSubtype: Abstract Control Management desc */
        0x02,   /* bmCapabilities */
/*Union Functional Descriptor*/
        0x05,   /* bFunctionLength */
        0x24,   /* bDescriptorType: CS_INTERFACE */
        0x06,   /* bDescriptorSubtype: Union func desc */
        0x00,   /* bMasterInterface: Communication class interface */
        0x01,   /* bSlaveInterface0: Data Class Interface */
/*Endpoint 2 Descriptor*/
        0x07,   /* bLength: Endpoint Descriptor size */
        5,   /* bDescriptorType: Endpoint */
        0x82,   /* bEndpointAddress: (IN2) */
        0x03,   /* bmAttributes: Interrupt */
        64,      /* wMaxPacketSize: */
        0x00,
        0xFF,   /* bInterval: */
/*Data class interface descriptor*/
        0x09,   /* bLength: Endpoint Descriptor size */
        4,  /* bDescriptorType: */
        0x01,   /* bInterfaceNumber: Number of Interface */
        0x00,   /* bAlternateSetting: Alternate setting */
        0x02,   /* bNumEndpoints: Two endpoints used */
        0x0A,   /* bInterfaceClass: CDC */
        0x00,   /* bInterfaceSubClass: */
        0x00,   /* bInterfaceProtocol: */
        0x00,   /* iInterface: */
/*Endpoint 3 Descriptor*/
        0x07,   /* bLength: Endpoint Descriptor size */
        5,   /* bDescriptorType: Endpoint */
        0x03,   /* bEndpointAddress: (OUT3) */
        0x02,   /* bmAttributes: Bulk */
        64,             /* wMaxPacketSize: */
        0x00,
        0x00,   /* bInterval: ignore for Bulk transfer */
/*Endpoint 1 Descriptor*/
        0x07,   /* bLength: Endpoint Descriptor size */
        5,   /* bDescriptorType: Endpoint */
        0x81,   /* bEndpointAddress: (IN1) */
        0x02,   /* bmAttributes: Bulk */
        64,             /* wMaxPacketSize: */
        0x00,
        0x00    /* bInterval: ignore for Bulk transfer */
};

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
  //uart_write_string("USB: Setting up EP!\n");

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
  //uart_write_string(" USB: Read data: ");
  uint32_t rxBufferAddr = USBBUFTABLE->ep_desc[ep].rxBufferAddr;
  for(int n=0; n<len; n++) {
    buffer[n] = USBBUFRAW[rxBufferAddr+n];
    //uart_write_int(buffer[n]);
    //uart_write_string(" ");
  }
  //uart_write_string("\n");
}

uint32_t ep_ready(uint32_t ep) {
  return((USB_EPR(ep) & 0x30) == 0x20);
}

void usb_write_packet(uint8_t ep, uint8_t * buffer, uint32_t len) {
  //uart_write_string(" USB: Write data: ");
  uint32_t txBufferAddr = USBBUFTABLE->ep_desc[ep].txBufferAddr;
  for(int n=0; n<len; n+=2) {
    USBBUFRAW16[(txBufferAddr+n)>>1] = buffer[n] | (buffer[n+1] << 8);
    //uart_write_int(buffer[n]);
    //uart_write_string(" ");
    //uart_write_int(buffer[n+1]);
    //uart_write_string(" ");
  }
  //uart_write_string("\n");
  USBBUFTABLE->ep_desc[ep].txBufferCount = len;

  // Clear CTR_TX and toggle NAK->VALID
  USB_EPR(ep) = (USB_EPR(ep) & 0x870f) | 0x0010;
  while(!ep_ready(0));
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
  if(bmRequestType == 0x80 && bRequest == 0x06) {
    //uart_write_string(" USB: GET_DESCRIPTOR ");
    uint8_t descriptor_type = packet[3];
    uint8_t descriptor_idx  = packet[2];
    //uart_write_int(descriptor_type);
    //uart_write_string(" ");
    //uart_write_int(descriptor_idx);
    //uart_write_string("\n");

    if(descriptor_type == 1 && descriptor_idx == 0) {
      usb_write(0, device_descriptor, 18, packet[6]);
    }
    if(descriptor_type == 2 && descriptor_idx == 0) {
      usb_write(0, config_descriptor, 67, packet[6]);
    }
  }
  if(bmRequestType == 0x00 && bRequest == 0x05) {
    //uart_write_string(" USB: SET_ADDRESS\n");
    pending_addr = packet[2];
    usb_write(0,0,0,0);
  }
  if(bmRequestType == 0x00 && bRequest == 0x09) {
    //uart_write_string(" USB: SET_CONFIGURATION\n");
    usb_write(0,0,0,0);
  }
}

void USB_IRQHandler() {
  if(USB->ISTR & USB_ISTR_RESET) {
    USB->ISTR &= ~USB_ISTR_RESET;
    buffer_pointer = 64;

    //uart_write_string("USB: Reset!\n");
    // Configure endpoint 0 for control
    usb_configure_ep(0, 1, 64);
        
    USB->BTABLE = 0;

    // Enable the peripheral
    USB->DADDR = USB_DADDR_EF;

  }
  
  if(USB->ISTR & USB_ISTR_CTR) {
    //uart_write_string("USB: Transfer complete\n");
    uint32_t ep = USB->ISTR & 0xf;
    if(USB->ISTR & 0x10) {
      // RX
      //uart_write_string(" USB: Packet received\n");
      if(USB_EPR(ep) & USB_EP_SETUP) {
        //uart_write_string(" USB: Processing setup packet\n");
        uint8_t setup_buf[8];
        usb_read(0, setup_buf, 8);
        handle_setup(setup_buf);
      } else {
        //uart_write_string(" USB: Processing data packet\n");
        uint32_t len = USBBUFTABLE->ep_desc[ep].rxBufferCount & 0x03ff;
        uint8_t data_buf[64];
        usb_read(ep, data_buf, len);
      }
      // Clear CTR_RX and toggle NAK->VALID
      USB_EPR(ep) = (USB_EPR(ep) & 0x078f) | 0x1000;
    } else {
      // TX
      //uart_write_string(" USB: Packet transmitted\n");
      USB_EPR(ep) = (USB_EPR(ep) & 0x870f);
    }

    if(pending_addr) {
      //uart_write_string(" USB: Updated address\n");
      USB->DADDR = USB_DADDR_EF | pending_addr;
      pending_addr = 0;
    }
  }

  if(USB->ISTR) {
    // uart_write_string("USB: Unknown interrupt: ");
    // uart_write_int(USB->ISTR);
    // uart_write_nl();
    //USB->ISTR = 0;
  }
}
