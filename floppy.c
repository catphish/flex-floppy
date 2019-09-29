#include <stm32l433xx.h>
#include <stdint.h>
#include "gpio.h"
#include "floppy.h"
#include "usb.h"
#include "util.h"

volatile extern uint8_t task;

int headpos = -1;
int target_track;

void floppy_init() {
  // TIM2
  RCC->APB1ENR1 |= RCC_APB1ENR1_TIM2EN;

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
  while(GPIOB->IDR & (1<<1))
    track_minus();
  headpos = 0;
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

void start_timer() {
  TIM2->CR1   = 0;
  TIM2->CCMR2 = 1;
  TIM2->CCER  = (1<<8) | (1<<9);
  TIM2->CNT   = 0;
  TIM2->CR1   = 1;
}

void floppy_handle_usb_request(uint8_t * packet, uint8_t length) {
  if(packet[0] == 1) {
    // Enable
    floppy_enable();
    motor_on();
  }
  if(packet[0] == 2) {
    // Disable
    motor_off();
    floppy_disable();
  }
  if(packet[0] == 4) {
    target_track = packet[1];
    task = 4;

  }
}

// Load one raw MFM bit into a raw sector
void record_bit(struct raw_sector * sector, char bit, uint32_t * syncword, int32_t * progress) {
  if(*progress == -1) {
    // Waiting for 32-bit sync
    *syncword = (*syncword << 1) | bit;
    if(*syncword == 0x44894489) {
      *progress = 0;
    }
  } else {
    // Dump raw MFM bits into data structure
    sector->data[*progress / 8] <<= 1;
    sector->data[*progress / 8] |= bit;
    (*progress)++;
  }
}

// Parse one raw sector into a decoded sector
void decode_sector(struct raw_sector * raw_sector, struct decoded_sector * decoded_sector, struct decoded_sector ** ordered_sectors) {
  decoded_sector->format    = ((raw_sector->data[0] & 0x55) << 1) | (raw_sector->data[4] & 0x55);
  decoded_sector->track     = ((raw_sector->data[1] & 0x55) << 1) | (raw_sector->data[5] & 0x55);
  decoded_sector->sector    = ((raw_sector->data[2] & 0x55) << 1) | (raw_sector->data[6] & 0x55);
  decoded_sector->remaining = ((raw_sector->data[3] & 0x55) << 1) | (raw_sector->data[7] & 0x55);
  for(int n = 0; n < 512; n++) {
    decoded_sector->data[n] = ((raw_sector->data[56+n] & 0x55) << 1) | (raw_sector->data[568+n] & 0x55);
  }
  ordered_sectors[decoded_sector->sector] = decoded_sector;
}

void floppy_read_track() {
  int32_t progress;
  int sector;
  uint32_t syncword;
  int prev_ccr3;

  // Zero the head if the current position is unknown
  if(headpos == -1) track_zero();

  // Seek
  uint8_t target_headpos = target_track / 2;
  while(headpos < target_headpos) track_plus();
  while(headpos > target_headpos) track_minus();
  set_side(target_track % 2);

  start_timer();
  progress = -1; // Waiting for sync
  syncword = 0;
  sector = 0;
  prev_ccr3 = 0;
  TIM2->SR = 0;

  struct raw_sector raw_sectors[11];
  struct decoded_sector decoded_sectors[11];
  struct decoded_sector * ordered_sectors[11];

  while(1) {
    if(TIM2->CNT > 80000000) {
      // 1-second timeout
      while(!ep_ready(0x82));
      usb_write_packet(0x82, (unsigned char *)"\1", 1);
      return;
    }
    if(TIM2->SR & (1<<3)) {
      // Timer fired
      int tval = TIM2->CCR3;
      unsigned int bits = (tval - prev_ccr3 + 80) / 160;
      prev_ccr3 = tval;

      while(bits) {
        record_bit(raw_sectors + sector, bits == 1, &syncword, &progress);
        bits--;
      }
    }
    if(progress >= 8640) {
      progress = -1;
      sector++;
      if(sector == 11) {
        // Done
        break;
      }
    }
  }

  // Success reading 11 raw sectors. Now decode.
  for(sector=0; sector < 11; sector++) {
    decode_sector(raw_sectors + sector, decoded_sectors + sector, ordered_sectors);
  }

  // Success decoding 11 sectors. Now send to USB.
  for(sector=0; sector < 11; sector++) {
    for(int bytes = 0; bytes < 512; bytes += 64) {
      while(!ep_ready(0x82));
      usb_write_packet(0x82, (unsigned char *)(ordered_sectors[sector]->data + bytes), 64);
    }
  }

  while(!ep_ready(0x82));
  usb_write_packet(0x82, (unsigned char *)"\0", 1);
}