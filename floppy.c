#include <stm32l433xx.h>
#include <stdint.h>
#include "gpio.h"
#include "floppy.h"
#include "usb.h"
#include "util.h"

volatile extern uint8_t task;

void floppy_init() {
  // TIM2
  RCC->APB1ENR1 |= RCC_APB1ENR1_TIM2EN;
  NVIC->ISER[0] |= (1 << TIM2_IRQn);

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
}

#define MEMORY_SIZE 8192
uint16_t data[MEMORY_SIZE];
uint16_t data_in_ptr  = 0;
uint16_t data_out_ptr = 0;

void track_minus() {
  GPIOB->BSRR = (1<<13);
  GPIOB->BSRR = (1<<(16+12));
  msleep(10);
  GPIOB->BSRR = (1<<12);
  msleep(10);
}

void track_plus() {
  GPIOB->BSRR = (1<<(16+13));
  GPIOB->BSRR = (1<<(16+12));
  msleep(10);
  GPIOB->BSRR = (1<<12);
  msleep(10);
}

void set_side(uint8_t side) {
  GPIOA->BSRR = (1 << (16 * side + 2));
}

void track_zero() {
  track_plus();
    while(GPIOB->IDR & (1<<1))
    track_minus();
}

void floppy_enable() {
  GPIOB->BSRR = (1<<(16+15)); // Enable
}
void floppy_disable() {
  GPIOB->BSRR = (1<<(15)); // Disable
}
void motor_on() {
  GPIOB->BSRR = (1<<(16+14)); // Motor on
}
void motor_off() {
  GPIOB->BSRR = (1<<(14)); // Motor off
}

int overflow;

void floppy_read_track() {
  data_in_ptr  = 0;
  data_out_ptr = 0;
  overflow = 0;

  TIM2->CR1   = 0;
  TIM2->CCMR2 = 1;
  TIM2->CCER  = (1<<8) | (1<<9);
  TIM2->CNT   = 0;
  TIM2->DIER  = (1<<3);
  TIM2->EGR   = (1<<3);
  TIM2->CR1   = 1;

  while(TIM2->CNT < 80000000) {
    if(overflow) {
      msleep(100);
      usb_write_packet(0x82, (unsigned char *)"\1", 1);
      return;
    }
    // Wait for a timer value
    if((data_in_ptr >= (data_out_ptr + 32)) || (data_in_ptr < data_out_ptr)) {
      if(ep_ready(0x82)) {
        usb_write_packet(0x82, (unsigned char *)(data+data_out_ptr), 64);
        data_out_ptr = data_out_ptr + 32;
        if(data_out_ptr == MEMORY_SIZE) data_out_ptr = 0;
      }
    }
  }
  TIM2->CR1   = 0;

  // EOF
  while(!ep_ready(0x82));
  usb_write_packet(0x82, (unsigned char *)"\0", 1);
}

uint8_t headpos = 0;

void floppy_handle_usb_request(uint8_t * packet, uint8_t length) {
  if(packet[0] == 1) {
    // Enable
    floppy_enable();
    motor_on();
    track_zero();
    headpos = 0;
    set_side(0);
  }
  if(packet[0] == 2) {
    // Disable
    motor_off();
    floppy_disable();
  }
  if(packet[0] == 3) {
    // Seek
    uint8_t target_track = packet[1];
    uint8_t target_headpos = target_track / 2;
    while(headpos < target_headpos) {
      track_plus();
      headpos++;
    }
    while(headpos > target_headpos) {
      track_minus();
      headpos--;
    }
    set_side(target_track % 2);
  }
  if(packet[0] == 4) {
    task = 4;
  }
}

void TIM2_IRQHandler() {
  data[data_in_ptr] = TIM2->CCR3;
  data_in_ptr++;
  if(data_in_ptr == data_out_ptr) overflow = 1;
  if(data_in_ptr == MEMORY_SIZE) data_in_ptr = 0;
}
