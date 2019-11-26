#include <stm32l433xx.h>
#include <stdint.h>
#include "gpio.h"
#include "floppy.h"
#include "usb.h"
#include "util.h"

//struct sector sectors[11];
uint8_t headpos;
uint8_t cached_track;

struct sector sectors[11];
struct sector * ordered_sectors[11];

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

  cached_track = -1;
  headpos = -1;

  floppy_enable();
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

void start_timer() {
  TIM2->CR1   = 0;
  TIM2->CCMR2 = 1;
  TIM2->CCER  = (1<<8) | (1<<9);
  TIM2->CNT   = 0;
  TIM2->CR1   = 1;
}

char * floppy_read_sector(uint16_t block) {
  uint16_t track  = block / 11;
  uint16_t sector = block % 11;
  
  if(cached_track == track) {
    for(int m=0; m<512; m++)
      ordered_sectors[sector]->assembled[m] = ((ordered_sectors[sector]->raw[64+m] & 0x55) << 1) | (ordered_sectors[sector]->raw[576+m] & 0x55);
    return(ordered_sectors[sector]->assembled);
  } else {
    cached_track = -1;
  }

  if(headpos == -1) floppy_track_zero();
  floppy_track_seek(track);
  uint32_t sector_bitmap = 0;
  struct sector * current_sector = sectors;

  int32_t progress = -1; // Waiting for sync
  uint32_t prev_ccr3 = 0; // Store previous timer value to calculate delta
  start_timer();
  TIM2->SR = 0;

  while(1) {
    if(TIM2->CNT > 50000000) {
      // 1-second timeout
      return 0; // Read error
    }
    if(TIM2->SR & (1<<3)) {
      // Timer fired
      int tval = TIM2->CCR3;
      unsigned int bits = (tval - prev_ccr3 + 80) / 160;
      prev_ccr3 = tval;

      while(bits) {
        if(progress == -1) {
          // Don't worry about bytes 0-3 during reading
          uint32_t * syncword = (uint32_t *)(current_sector->raw + 4);
          // Waiting for 32-bit sync
          *syncword <<= 1;
          *syncword |= (bits == 1);
          if(*syncword == 0x44894489) {
            progress = 64; // 0x8 * 8-bits
          }
          current_sector->header_checksum = 0;
          current_sector->data_checksum = 0;
        } else {
          // Dump raw MFM bits into data structure
          current_sector->raw[progress / 8] <<= 1;
          current_sector->raw[progress / 8] |= (bits == 1);
          progress++;

          // Generate checksums as we go along
          if(progress % 32 == 0) {
            uint32_t previous_long = *(uint32_t*)(current_sector->raw + (progress / 8 - 4));
            if(progress > 8*8 && progress <= 48*8) {
              current_sector->header_checksum ^= previous_long;
            }
            if(progress > 64*8) {
              current_sector->data_checksum ^= previous_long;
            }
          }

          if(progress == 8704) break;
        }
        bits--;
      }
    }
    if(progress == 8704) {
      // End of sector

      // Compare the even checksum bits, the odd bits are just MFM clock
      uint32_t header_checksum = *(uint32_t*)(current_sector->raw + 52);
      uint32_t data_checksum   = *(uint32_t*)(current_sector->raw + 60);
      header_checksum &= 0x55555555;
      data_checksum   &= 0x55555555;
      current_sector->header_checksum &= 0x55555555;
      current_sector->data_checksum   &= 0x55555555;

      if(header_checksum == current_sector->header_checksum) {
        if(data_checksum == current_sector->data_checksum) {
          // Got a good sector
          uint8_t sector_number = ((current_sector->raw[10] & 0x55) << 1) | (current_sector->raw[14] & 0x55);

          if(sector_number < 11 && (sector_bitmap & (1 << sector_number)) == 0) {
            // We need this piece, record it.
            ordered_sectors[sector_number] = current_sector;
            current_sector++;
            sector_bitmap |= (1 << sector_number);
          }
          if(sector_bitmap == 0b11111111111) {
            // We have all the pieces. Yay.
            cached_track = track;
            // floppy_disable();
            for(int m=0; m<512; m++)
              ordered_sectors[sector]->assembled[m] = ((ordered_sectors[sector]->raw[64+m] & 0x55) << 1) | (ordered_sectors[sector]->raw[576+m] & 0x55);
            return(ordered_sectors[sector]->assembled);
          }

        }
      }
      progress = -1;
    }
  }

}