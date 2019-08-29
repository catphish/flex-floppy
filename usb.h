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
