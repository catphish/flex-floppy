uint32_t ep_ready(uint32_t ep);
uint32_t ep_rx_ready(uint32_t ep);
void usb_read(uint8_t ep, char * buffer);
void usb_write(uint8_t ep, volatile char * buffer, uint32_t len);
void usb_init();
