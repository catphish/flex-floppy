#define USB_EPR(n) (*(volatile uint16_t *)(USB_BASE + 4 * n))
#define USBBUFTABLE ((volatile struct USBBufTable *)0x40006C00)
#define USBBUFRAW ((volatile uint8_t *)0x40006C00)
#define USBBUFRAW16 ((volatile uint16_t *)0x40006C00)

struct USBBufTable {
  struct USBBufDesc {
    uint16_t txBufferAddr ;
    uint16_t txBufferCount ;
    uint16_t rxBufferAddr ;
    uint16_t rxBufferCount ;
  }
  ep_desc[8];
};

// Totally generic device descriptor
uint8_t device_descriptor[] = {
  18,
  1,
  0x10,0x01,
  0, 0, 0,
  64,
  0x09,0x12,
  0x01,0x00,
  1,0,
  0,0,0,
  1
};

// Describes a standard virtual COM port
uint8_t config_descriptor[] = {
/*Configuation Descriptor*/
  0x09,   /* bLength: Configuation Descriptor size */
  2,      /* bDescriptorType: Configuration */
  9 +9+5+4+5+5+7 +9+7+7,     /* wTotalLength:no of returned bytes */
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
  0x00,   /* bInterfaceProtocol: Common AT commands */
  0x00,   /* iInterface: */

  0x05,
  0x24,
  0x00,
  0x10,0x01,

  0x04,
  0x24,
  0x02,
  0x02,

  0x05,
  0x24,
  0x06,
  0x00,
  0x01,

  0x05,
  0x24,
  0x01,
  0x00,
  0x01,

/*Interrupt Endpoint Descriptor*/
  0x07,   /* bLength: Endpoint Descriptor size */
  5,   /* bDescriptorType: Endpoint */
  0x81,   /* bEndpointAddress: (IN1) */
  0x03,   /* bmAttributes: Interrupt */
  64,     /* wMaxPacketSize: */
  0x00,
  0xff,   /* bInterval: */

/*Interface Descriptor*/
  0x09,   /* bLength: Interface Descriptor size */
  4,      /* bDescriptorType: Interface */
                  /* Interface descriptor type */
  0x01,   /* bInterfaceNumber: Number of Interface */
  0x00,   /* bAlternateSetting: Alternate setting */
  0x02,   /* bNumEndpoints: One endpoints used */
  0x0A,   /* bInterfaceClass: Communication Interface Class */
  0x00,   /* bInterfaceSubClass: Abstract Control Model */
  0x00,   /* bInterfaceProtocol: Common AT commands */
  0x00,   /* iInterface: */

/*RX Endpoint Descriptor*/
  0x07,   /* bLength: Endpoint Descriptor size */
  5,   /* bDescriptorType: Endpoint */
  0x02,   /* bEndpointAddress: (OUT2) */
  0x02,   /* bmAttributes: Interrupt */
  64,     /* wMaxPacketSize: */
  0x00,
  0x00,   /* bInterval: */
/*TX Endpoint Descriptor*/
  0x07,   /* bLength: Endpoint Descriptor size */
  5,   /* bDescriptorType: Endpoint */
  0x82,   /* bEndpointAddress: (IN2) */
  0x02,   /* bmAttributes: Interrupt */
  64,     /* wMaxPacketSize: */
  0x00,
  0x00,   /* bInterval: */
};
