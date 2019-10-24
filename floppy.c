#include <stm32l433xx.h>
#include <stdint.h>
#include "gpio.h"
#include "floppy.h"
#include "usb.h"
#include "util.h"

volatile uint32_t previous_timer_value;
volatile uint32_t read_length;
volatile uint16_t status;
volatile uint8_t headpos;
volatile uint8_t task;

#define MEMORY_SIZE (1024*16)
volatile uint16_t data[MEMORY_SIZE];
volatile uint16_t data_in_ptr;
volatile uint16_t data_out_ptr;
volatile uint32_t data_total_ptr;

#define INDEX_PULSE_PACKETS 8
#define MAX_INDEX_PULSES (INDEX_PULSE_PACKETS * 8)
volatile uint16_t index_pulse_ptr;
volatile struct index_pulse index_pulses[MAX_INDEX_PULSES];

void floppy_init() {
  // TIM2
  RCC->APB1ENR1 |= RCC_APB1ENR1_TIM2EN;
  NVIC->ISER[0] |= (1 << TIM2_IRQn);
  NVIC->IP[TIM2_IRQn] = 1;

  // EXTI for INDEX on A8
  EXTI->FTSR1 |= (1<<8);
  NVIC->ISER[0] |= (1 << EXTI9_5_IRQn);

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
  EXTI->IMR1  = 0;

  // Set prescaler
  TIM2->PSC   = 1;
  TIM2->EGR   = 1;

  // Configure TIM2 CH3 as input capture
  TIM2->CCMR2 = 1;
  TIM2->CCER  = (1<<8) | (1<<9);
  TIM2->CNT   = 0;
  TIM2->ARR   = 0xffffffff;

  // Zero the counters
  previous_timer_value = 0;
  data_in_ptr   = 0;
  data_out_ptr  = 0;
  index_pulse_ptr = 0;
  data_total_ptr  = 0;

  // Flush index pulses
  for(int n=0; n<MAX_INDEX_PULSES; n++) {
    index_pulses[n].time = 0;
    index_pulses[n].data_ptr = 0;
  }

  // Set current task and clear status
  task   = 0x21;
  status = 0;

  // Enable EXTI
  EXTI->IMR1 = (1<<8);

  // Reset interrupts and enable TIM2
  TIM2->SR    = 0;
  TIM2->CR1   = 1;
  TIM2->DIER  = (1<<3);
}

void floppy_start_write() {
  // Disable TIM2 and interrupts
  TIM2->CR1   = 0;
  TIM2->DIER  = 0;

  // Set prescaler
  TIM2->PSC   = 1;
  TIM2->EGR   = 1;

  // Configure TIM2 CH4 as PWM output (mode 1)
  TIM2->CCMR2 = (6<<12);
  TIM2->CCER  = (1<<12) | (1<<13);
  TIM2->CNT   = 0;
  TIM2->CCR4  = 160; // 2us pulse
  TIM2->ARR   = 0xffffffff;

  // Reset counters
  data_in_ptr = 0;
  data_out_ptr = 0;

  // Set current task and clear status
  task   = 0x31;
  status = 0;
}

void floppy_really_start_write() {
  // Set current task
  task   = 0x32;

  // Arm the death ray
  floppy_write_enable();

  // Enable timer
  TIM2->DIER  = 1;
  TIM2->SR    = 0;
  TIM2->CR1   = 1;
  TIM2->EGR   = 1;
}

// This is the main loop.
// It handles general operation monitoring and USB transmit.
// USB receive and timers are handled in separate functions.
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
      // Disable TIM2 and interrupts
      TIM2->CR1   = 0;
      TIM2->DIER  = 0;
      EXTI->IMR1  = 0;

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
        task = 0x23;
        index_pulse_ptr = 0;
      }
    }
  }

  if(task == 0x23) { // Done sending, send index pulse metadata
    if(usb_tx_ready(0x81)) {
      if(index_pulse_ptr < MAX_INDEX_PULSES) {
        usb_write(0x81, (char *)(index_pulses + index_pulse_ptr), 64);
        index_pulse_ptr += 8;
      } else {
        usb_write(0x81, 0, 0);
        task = 0x24;
      }
    }
  }

  if(task == 0x24) { // Finally send the transfer status
    if(usb_tx_ready(0x81)) {
      usb_write(0x81, (char *)&status, 2);
      task = 0;
    }
  }

  if(task == 0x32) { // Writing
    if(status) {
      // Disable TIM2 and interrupts
      TIM2->CR1   = 0;
      TIM2->DIER  = 0;
      // Advance to next task
      task = 0x33;
    }
  }

  if(task == 0x33) { // Confirm write status
    if(usb_tx_ready(0x81)) {
      usb_write(0x81, (char *)&status, 2);
      task = 0;
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
    floppy_start_write();
    usb_write(0,0,0);
  }
}

void floppy_handle_ep1() {
  if(task == 0x31) {
    // Receive data stream and pre-fill ring buffer
    usb_read(0x01, (char *)(data + data_in_ptr));
    data_in_ptr += 32;

    if(data_in_ptr == MEMORY_SIZE) {
      // Buffer full. Start writing.
      data_in_ptr = 0;
      floppy_really_start_write();
    }
  } else if(task == 0x32) {
    // Receive data stream and continue to write disk
    if((data_out_ptr >= (data_in_ptr + 32)) || (data_out_ptr < data_in_ptr)) {
      usb_read(0x01, (char *)(data + data_in_ptr));
      data_in_ptr = (data_in_ptr + 32) % MEMORY_SIZE;
    }
  } else {
    usb_read(0x01, 0); // Discard
  }
}

void TIM2_IRQHandler() {
  TIM2->SR = 0;

  if(task == 0x32) { // Write to TIM2->ARR
    uint16_t t = data[data_out_ptr];
    if(t) {
      TIM2->ARR = t;
    } else { // EOF
      floppy_write_disable();
      status = 1;
    }
    
    data_out_ptr = (data_out_ptr + 1) % MEMORY_SIZE;
    if(!status && data_out_ptr == data_in_ptr) {
      floppy_write_disable();
      status = 2;      
    }
  }

  if(task == 0x21) { // Read from TIM2->CH3
    if(!status) {
      data[data_in_ptr] = TIM2->CCR3;
      data_in_ptr = (data_in_ptr + 1) % MEMORY_SIZE;
      data_total_ptr++;
    }
  }
}

void EXTI9_5_IRQHandler() {
  // Acknowledge interrupt
  EXTI->PR1 = (1<<8);
  // INDEX pulse detected
  if(index_pulse_ptr < MAX_INDEX_PULSES) {
    index_pulses[index_pulse_ptr].time = TIM2->CNT;
    index_pulses[index_pulse_ptr].data_ptr = data_total_ptr;
    index_pulse_ptr++;
  }
}
