#include <stm32l433xx.h>
#include <stdint.h>
#include "gpio.h"
#include "floppy.h"
#include "usb.h"
#include "util.h"

volatile extern uint8_t task;
volatile uint32_t previous_timer_value;
volatile uint8_t headpos = 0;
volatile uint8_t overflow;

volatile char data[MEMORY_SIZE];
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
}

void track_minus() {
  GPIOB->BSRR = (1<<13);
  GPIOB->BSRR = (1<<(16+12));
  msleep(10);
  GPIOB->BSRR = (1<<12);
  msleep(10);
  headpos--;
}

void track_plus() {
  GPIOB->BSRR = (1<<(16+13));
  GPIOB->BSRR = (1<<(16+12));
  msleep(10);
  GPIOB->BSRR = (1<<12);
  msleep(10);
  headpos++;
}

void set_side(uint8_t side) {
  GPIOA->BSRR = (1 << (16 * side + 2));
}

void track_zero() {
  track_plus();
  while(GPIOB->IDR & (1<<1)) track_minus();
  headpos = 0;
}

void track_seek(uint8_t target) {
  uint8_t target_headpos = target / 2;
  while(headpos < target_headpos) track_plus();
  while(headpos > target_headpos) track_minus();
  set_side(target % 2);
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

void disable_timer() {
  // Disable TIM2 and interrupts
  TIM2->CR1   = 0;
  TIM2->DIER  = 0;
}

void floppy_start_read() {
  disable_timer();

  // Configure TIM2 CH3 as input capture
  TIM2->CCMR2 = 1;
  TIM2->CCER  = (1<<8) | (1<<9);
  TIM2->CNT   = 0;
  TIM2->ARR   = 0xffffffff;

  // Zero the counters
  previous_timer_value = 0;
  data_in_ptr   = 0;
  data_out_ptr  = 0;
  overflow      = 0;

  // Reset interrupts and enable TIM2
  TIM2->SR    = 0;
  TIM2->CR1   = 1;
  TIM2->DIER  = (1<<3);
}

void floppy_prepare_write() {
  disable_timer();

  // Configure TIM2 CH4 as PWM output (mode 1)
  TIM2->CCMR2 = (6<<12);
  TIM2->CCER  = (1<<12) | (1<<13);
  TIM2->CNT   = 0;
  TIM2->CCR4  = 160; // 2us pulse

  // Zero the counters
  data_in_ptr   = 0;
  data_out_ptr  = 0;
  overflow      = 0;
}

void floppy_start_write() {
  // Arm the death ray
  floppy_write_enable();
  // Enable timer
  TIM2->ARR   = 0xffffffff;
  TIM2->DIER  = 1;
  TIM2->SR    = 0;
  TIM2->CR1   = 1;
  TIM2->EGR   = 1;
}

static inline void increment_data_in_ptr() {
  data_in_ptr = (data_in_ptr + 1) % MEMORY_SIZE;
  if(data_in_ptr == data_out_ptr) overflow = 1;
}

static inline void increment_data_out_ptr() {
  data_out_ptr = (data_out_ptr + 1) % MEMORY_SIZE;
  if(data_out_ptr == data_in_ptr) overflow = 1;
}

void terminate_data() {
  data[data_in_ptr] = 0;
  increment_data_in_ptr();
  data[data_in_ptr] = overflow;
  increment_data_in_ptr();
}

void TIM2_IRQHandler() {
  if(task == 8) {
    // Write to TIM2->CH4 (actually we set TIM2->ARR)
    uint8_t b0 = ((uint8_t*)data)[data_out_ptr];
    increment_data_out_ptr();
    if(b0 > 0x0f) {
      if(b0 < 120) {
        TIM2->ARR = 120;
      } else {
        TIM2->ARR = b0;
      }
    } else if(b0 == 0x0f) {
      uint32_t tval;
      tval  = ((uint8_t*)data)[data_out_ptr]; tval <<= 8;
      increment_data_out_ptr();
      tval |= ((uint8_t*)data)[data_out_ptr]; tval <<= 8;
      increment_data_out_ptr();
      tval |= ((uint8_t*)data)[data_out_ptr]; tval <<= 8;
      increment_data_out_ptr();
      tval |= ((uint8_t*)data)[data_out_ptr];
      increment_data_out_ptr();
      TIM2->ARR = tval;
    } else if(b0 > 0) {
      uint32_t tval;
      tval  = b0; tval <<= 8;
      tval |= ((uint8_t*)data)[data_out_ptr];
      increment_data_out_ptr();
      TIM2->ARR = tval;
    }
    if((b0 == 0) || overflow ) {
      TIM2->CR1  = 0;
      TIM2->DIER = 0;
      floppy_write_disable();
      task = 9;
    }
  }

  if(task == 5) {
    // Read from TIM2->CH3
    uint32_t timer_value = TIM2->CCR3;
    if(previous_timer_value) {
      uint32_t delta = timer_value - previous_timer_value;
      if(delta > 3839) {
        // Send as 32-bit value
        data[data_in_ptr] = 0xf;
        increment_data_in_ptr();
        data[data_in_ptr] = (delta & 0xff000000) >> 24;
        increment_data_in_ptr();
        data[data_in_ptr] = (delta & 0x00ff0000) >> 16;
        increment_data_in_ptr();
        data[data_in_ptr] = (delta & 0x0000ff00) >> 8;
        increment_data_in_ptr();
        data[data_in_ptr] = (delta & 0x000000ff) >> 0;
        increment_data_in_ptr();
      } else if (delta > 255) {
        // Send as 16 bit value
        data[data_in_ptr] = (delta & 0x0000ff00) >> 8;
        increment_data_in_ptr();
        data[data_in_ptr] = (delta & 0x000000ff) >> 0;
        increment_data_in_ptr();
      } else if(delta > 15) {
        // Send as 8 bit value
        data[data_in_ptr++] = delta;
        increment_data_in_ptr();
      }
    }
    previous_timer_value = timer_value;
  }
  TIM2->SR    = 0;
}
