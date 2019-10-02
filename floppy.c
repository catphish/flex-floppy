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
  GPIOB->BSRR = (1<<(16+14)); // Motor on
}
void floppy_disable() {
  GPIOB->BSRR = (1<<(14)); // Motor off
  GPIOB->BSRR = (1<<(15)); // Disable
}

void start_timer() {
  TIM2->CR1   = 0;
  TIM2->CCMR2 = 1;
  TIM2->CCER  = (1<<8) | (1<<9);
  TIM2->CNT   = 0;
  TIM2->CR1   = 1;
}

void floppy_handle_usb_request(char * packet, uint8_t length) {
  if(packet[0] == 1) {
    floppy_enable();
  }
  if(packet[0] == 2) {
    floppy_disable();
  }
  if(packet[0] == 4) {
    target_track = packet[1];
    task = 4;
  }
}

void floppy_read_track() {
  // Zero the head if the current position is unknown
  if(headpos == -1) track_zero();

  // Seek
  uint8_t target_headpos = target_track / 2;
  while(headpos < target_headpos) track_plus();
  while(headpos > target_headpos) track_minus();
  set_side(target_track % 2);

  start_timer();
  uint32_t sector_bitmap = 0; // Bitmap of successfully read sectors
  int32_t progress = -1; // Waiting for sync
  uint32_t prev_ccr3 = 0; // Store previous timer value to calculate delta
  TIM2->SR = 0;

  struct sector sectors[11];
  struct sector * current_sector = sectors;
  struct sector * ordered_sectors[11];

  while(1) {
    if(TIM2->CNT > 80000000) {
      // 1-second timeout
      while(!ep_ready(0x81));
      usb_write_packet(0x81, "\1", 1);
      return;
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
            for(int n=0; n<11; n++) {
              for(int m=0; m<512; m+=64) {
                char sector_out[64];
                for(int j=0; j<64; j++) {
                  sector_out[j] = ((ordered_sectors[n]->raw[64+m+j] & 0x55) << 1) | (ordered_sectors[n]->raw[576+m+j] & 0x55);
                }
                while(!ep_ready(0x81));
                usb_write_packet(0x81, sector_out, 64);
              }
            }
            while(!ep_ready(0x81));
            usb_write_packet(0x81, "\0", 1);
            return;
          }

        }
      }
      progress = -1;
    }
  }

}