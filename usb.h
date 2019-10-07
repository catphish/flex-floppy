#define USB_EPR(n) (*(volatile uint16_t *)(USB_BASE + 4 * n))
uint32_t ep_ready(uint32_t ep);
uint32_t ep_rx_ready(uint32_t ep);
void usb_read(uint8_t ep, char * buffer);
void usb_write(uint8_t ep, char * buffer, uint32_t len);
void usb_write_dbl(uint8_t ep, char * buffer, uint32_t len);
void usb_init();
