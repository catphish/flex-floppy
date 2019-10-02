uint32_t ep_ready(uint32_t ep);
uint32_t ep_rx_ready(uint32_t ep);
void usb_read(uint8_t ep, char * buffer, uint32_t len);
void usb_write_packet(uint8_t ep, char * buffer, uint32_t len);
void usb_write(uint8_t ep, char * buffer, uint32_t len, uint32_t limit_len);
void usb_init();
void msc_handle_usb_request(char * packet, uint8_t length);
