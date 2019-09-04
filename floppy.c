#include <stm32l433xx.h>
#include <stdint.h>
#include "gpio.h"
#include "floppy.h"
#include "usb.h"
#include "util.h"

void floppy_init() {
  // TIM2
  RCC->APB1ENR1 |= RCC_APB1ENR1_TIM2EN;
  TIM2->CCMR2 = 1;
  TIM2->CCER  = (1<<8) | (1<<9);
  TIM2->CR1   = 1;

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

uint16_t data[16*1024];
uint32_t data_in_ptr = 0;
uint32_t data_out_ptr = 0;

void track_minus() {
  GPIOB->BSRR = (1<<13);
  GPIOB->BSRR = (1<<(16+12));
  usleep(100000);
  GPIOB->BSRR = (1<<12);
  usleep(100000);
}

void track_plus() {
  GPIOB->BSRR = (1<<(16+13));
  GPIOB->BSRR = (1<<(16+12));
  usleep(100000);
  GPIOB->BSRR = (1<<12);
  usleep(100000);
}

void track_zero() {
  track_plus();
    while(GPIOB->IDR & (1<<1))
    track_minus();
}

void floppy_enable() {
  GPIOB->BSRR = (1<<(16+15)); // Enable
}
void motor_on() {
  GPIOB->BSRR = (1<<(16+14)); // Motor on
}
void motor_off() {
  GPIOB->BSRR = (1<<(14)); // Motor off
}

void floppy_read_track() {
  floppy_enable();
  motor_on();
  track_zero();

  for(int n=0; n<20;n++) usleep(100000);

  volatile uint32_t previous_t;

  TIM2->CNT = 0;

  while(!(TIM2->SR & (1<<3)));
  previous_t = TIM2->CCR3;
  while(!(TIM2->SR & (1<<3)));
  previous_t = TIM2->CCR3;

  while(TIM2->CNT < 80000000) {
    // Wait for a timer value
    while(!(TIM2->SR & (1<<3)));
    uint32_t current_t = TIM2->CCR3;
    data[data_in_ptr] = current_t - previous_t;
    data_in_ptr++;
    previous_t = current_t;
    if(data_in_ptr == 16*1024) data_in_ptr = 0;

    if((data_in_ptr >= data_out_ptr + 32) || (data_in_ptr < data_out_ptr)) {
      if(ep_ready(0x81)) {
        //usb_write(0x81, (unsigned char *)data + data_out_ptr, 64, 64);
        usb_write(0x81, (unsigned char *)data + data_out_ptr, 64, 64);
        data_out_ptr = data_out_ptr + 32;
        if(data_out_ptr == 16*1024) data_out_ptr = 0;
      }
    }
  }
  motor_off();
}
