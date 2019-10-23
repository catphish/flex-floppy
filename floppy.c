#include <stm32l433xx.h>
#include <stdint.h>
#include "gpio.h"
#include "floppy.h"
#include "usb.h"
#include "util.h"

volatile uint32_t previous_timer_value;
volatile uint32_t read_length;
volatile uint8_t headpos;
volatile uint8_t task;
volatile uint8_t status;

#define MEMORY_SIZE 8192
volatile uint16_t data[MEMORY_SIZE];
volatile uint16_t data_in_ptr  = 0;
volatile uint16_t data_out_ptr = 0;

void floppy_init() {
  // TIM2
  RCC->APB1ENR1 |= RCC_APB1ENR1_TIM2EN;
  NVIC->ISER[0] |= (1 << TIM2_IRQn);
  NVIC->IP[TIM2_IRQn] = 1;

  gpio_port_mode(GPIOA, 1, 0, 0, 1, 0);  // A1  READY     IN
  gpio_port_mode(GPIOA, 2, 1, 0, 0, 1);  // A2  HEADSELET OUT
  gpio_port_mode(GPIOA, 8, 0, 0, 1, 0);  // A8  INDEX     IN
  gpio_port_mode(GPIOA, 9, 1, 0, 0, 1);  // A9  DENSITY   OUT

  gpio_port_mode(GPIOB, 0, 0, 0, 1, 0);  // B0  PROTECT   IN
  gpio_port_mode(GPIOB, 1, 0, 0, 1, 0);  // B1  TRACK0    IN
  gpio_port_mode(GPIOB, 2, 1, 0, 0, 1);  // B2  WENABLE   OUT

  gpio_port_mode(GPIOB, 10, 2, 1, 1, 0); // B10 RDATA     TIMER IN
  gpio_port_mode(GPIOB, 11, 2, 1, 0, 1); // B11 WDATA     TIMER OUT

  gpio_port_mode(GPIOB, 12, 1, 0, 0, 1); // B12 STEP      OUT
  gpio_port_mode(GPIOB, 13, 1, 0, 0, 1); // B13 DIR       OUT

  gpio_port_mode(GPIOB, 14, 1, 0, 0, 1); // B14 MOTOR     OUT
  gpio_port_mode(GPIOB, 15, 1, 0, 0, 1); // B15 ENABLE    OUT

  task = 0;
}

void floppy_track_minus() {
  GPIOB->BSRR = (1<<13);
  GPIOB->BSRR = (1<<(16+12));
  msleep(10);
  GPIOB->BSRR = (1<<12);
  msleep(10);
  headpos--;
}

void floppy_track_plus() {
  GPIOB->BSRR = (1<<(16+13));
  GPIOB->BSRR = (1<<(16+12));
  msleep(10);
  GPIOB->BSRR = (1<<12);
  msleep(10);
  headpos++;
}

void floppy_set_side(uint8_t side) {
  GPIOA->BSRR = (1 << (16 * side + 2));
}

void floppy_track_zero() {
  floppy_track_plus();
  while(GPIOB->IDR & (1<<1)) floppy_track_minus();
  headpos = 0;
}

void floppy_track_seek(uint8_t target) {
  uint8_t target_headpos = target / 2;
  while(headpos < target_headpos) floppy_track_plus();
  while(headpos > target_headpos) floppy_track_minus();
  floppy_set_side(target % 2);
}

void floppy_enable() {
  GPIOB->BSRR = (1<<(16+15)); // Enable
  GPIOB->BSRR = (1<<(16+14)); // Motor on
}
void floppy_disable() {
  GPIOB->BSRR = (1<<(14)); // Motor off
  GPIOB->BSRR = (1<<(15)); // Disable
}
void floppy_write_enable() {
  GPIOB->BSRR = (1<<(16+2)); // Write enable!
}
void floppy_write_disable() {
  GPIOB->BSRR = (1<<(2));    // Write disable!
}

void floppy_start_read() {
  // Disable TIM2 and interrupts
  TIM2->CR1   = 0;
  TIM2->DIER  = 0;

  // Configure TIM2 CH3 as input capture
  TIM2->CCMR2 = 1;
  TIM2->CCER  = (1<<8) | (1<<9);
  TIM2->CNT   = 0;
  TIM2->ARR   = 0xffffffff;

  // Zero the counters
  previous_timer_value = 0;
  data_in_ptr   = 0;
  data_out_ptr  = 0;

  // Set current task and clear status
  task   = 0x21;
  status = 0;

  // Reset interrupts and enable TIM2
  TIM2->SR    = 0;
  TIM2->CR1   = 1;
  TIM2->DIER  = (1<<3);
}

void floppy_main_loop() {
  if(task == 0x21) { // Reading
    // Stop reading after fixed interval
    if(TIM2->CNT >= read_length) status = 1;

    // Check if we're approaching an overflow
    int32_t free = (int32_t)data_out_ptr - (int32_t)data_in_ptr;
    if(free < 0) free += MEMORY_SIZE;
    if(free && free < 16) status = 2;

    // Check if we have data to send
    if(usb_tx_ready(0x81)) {
      if((data_in_ptr >= (data_out_ptr + 32)) || (data_in_ptr < data_out_ptr)) {
        usb_write(0x81, (char *)(data + data_out_ptr), 64);
        data_out_ptr = (data_out_ptr + 32) % MEMORY_SIZE;
      }
    }

    if(status) {
      // Terminate stream with status
      data[data_in_ptr] = status;
      data_in_ptr = (data_in_ptr + 1) % MEMORY_SIZE;
      // Disable TIM2 and interrupts
      TIM2->CR1   = 0;
      TIM2->DIER  = 0;
      // Advance to next task
      task = 0x22;
    }
  }

  if(task == 0x22) { // Done reading, finish streaming.
    // Finish streaming data to USB
    if(usb_tx_ready(0x81)) {
      if((data_in_ptr >= (data_out_ptr + 32)) || (data_in_ptr < data_out_ptr)) {
        usb_write(0x81, (char *)(data + data_out_ptr), 64);
        data_out_ptr = (data_out_ptr + 32) % MEMORY_SIZE;
      } else {
        uint16_t remaining_data = data_in_ptr - data_out_ptr;
        usb_write(0x81, (char *)(data + data_out_ptr), remaining_data * 2);
        task = 0;
      }
    }
  }
}

void floppy_handle_ep0(char * packet) {
  uint8_t bRequest = packet[1];

  // Enable Drive
  if(bRequest == 0x01) {
    task = 0;
    floppy_enable();
    usb_write(0,0,0);
  }
  // Disble Drive
  if(bRequest == 0x02) {
    task = 0;
    floppy_disable();
    usb_write(0,0,0);
  }
  // Zero Head
  if(bRequest == 0x11) {
    // This is a slow blocking operation!
    floppy_track_zero();
    usb_write(0,0,0);
  }  // Seek Head
  if(bRequest == 0x12) {
    // This is a slow blocking operation!
    floppy_track_seek(packet[2]);
    usb_write(0,0,0);
  }
  // Read Track
  if(bRequest == 0x21) {
    read_length  = (uint32_t)packet[2] * 1000000;
    floppy_start_read();
    usb_write(0,0,0);
  }
  // Write Track
  if(bRequest == 0x31) {
    //floppy_prepare_write();
    usb_write(0,0,0);
  }
}

void floppy_handle_ep1() {

}

void TIM2_IRQHandler() {
  TIM2->SR = 0;

  if(task == 0x21) { // Read from TIM2->CH3
    data[data_in_ptr] = TIM2->CCR3;
    data_in_ptr = (data_in_ptr + 1) % MEMORY_SIZE;
  }
}
