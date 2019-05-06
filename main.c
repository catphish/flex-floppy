#include <stm32l476xx.h>
#include <stdint.h>
#include "uart.h"
#include "amiga.h"

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

// Read one raw sector from disk
void read_sector(struct raw_sector * raw_sector) {
  int32_t progress = -1;
  uint32_t syncword = 0;
  while(progress < 8640) {
    while(!(TIM1->SR & 2));
    unsigned int bits = (TIM1->CCR1 + 80) / 160;
    while(bits) {
      record_bit(raw_sector, bits == 1, &syncword, &progress);
      bits--;
    }
  }
}

// Parse one raw sector into a decoded sector
void decode_sector(struct raw_sector * raw_sector, struct decoded_sector * decoded_sector) {
  decoded_sector->format    = ((raw_sector->data[0] & 0x55) << 1) | (raw_sector->data[4] & 0x55);
  decoded_sector->track     = ((raw_sector->data[1] & 0x55) << 1) | (raw_sector->data[5] & 0x55);
  decoded_sector->sector    = ((raw_sector->data[2] & 0x55) << 1) | (raw_sector->data[6] & 0x55);

  uart_write_string("Decoded Sector.");
  uart_write_string(" Format: ");    uart_write_int(decoded_sector->format);
  uart_write_string(" Track: ");     uart_write_int(decoded_sector->track);
  uart_write_string(" Sector: ");    uart_write_int(decoded_sector->sector);
  uart_write_nl();
}


int main() {
  struct raw_sector raw_sectors[11];
  for(int n=0; n<11; n++)
    read_sector(raw_sectors + n);

  struct decoded_sector decoded_sectors[11];
  for(int n=0; n<11; n++)
    decode_sector(raw_sectors + n, decoded_sectors + n);

  while(1);
}
