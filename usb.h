#define USB_EPR(n) (*(volatile uint16_t *)(USB_BASE + 4 * n))

void usb_init();
void usb_configure_ep(uint8_t ep, uint32_t type);
uint32_t usb_tx_ready(uint32_t ep);
void usb_read(uint8_t ep, volatile char * buffer);
void usb_write(uint8_t ep, volatile char * buffer, uint32_t len);
void usb_reset();
void usb_handle_tx_complete();
void usb_handle_ep0();
void usb_main_loop();
