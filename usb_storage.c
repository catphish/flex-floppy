#include <stm32l433xx.h>
#include <stdint.h>
#include "usb_storage.h"
#include "usb_storage.hh"
#include "usb.h"
#include "floppy.h"
#include "util.h"

struct sense_data sense_data;

void usb_storage_write_stream(char * buffer, uint32_t len, uint8_t terminate) {
  while(len >= 64) {
    usb_write(0x81, buffer, 64);
    buffer += 64;
    len -= 64;
  }
  if(terminate)
    usb_write(0x81, buffer, len);
}

void usb_storage_scsi_respond_status(uint32_t tag, uint8_t status, uint8_t sense_key, uint8_t additional_sense_code, uint8_t additional_sense_code_qualifier) {
  // Set sense data
  sense_data.response_code = 0x70;
  sense_data.additional_length = 10;
  sense_data.sense_key = sense_key;
  sense_data.additional_sense_code = additional_sense_code;
  sense_data.additional_sense_code_qualifier = additional_sense_code_qualifier;

  // Generate CSW
  struct command_status_wrapper csw;
  csw.signature = 0x53425355;
  csw.tag = tag;
  csw.data_residue = 0;
  csw.status = status;
  usb_storage_write_stream((char*)&csw, sizeof(csw), 1);
}

void usb_storage_handle_ep1() {
  char packet[64];
  usb_read(1, packet);
  struct command_block_wrapper * command = (struct command_block_wrapper *)&packet;
  uint16_t allocation_length;
  uint32_t start_block;
  uint32_t length;

  switch(command->data[0]) {
    case 0x12:
      allocation_length = command->data[4];
      // INQUIRY
      if(command->data[1] & 3 || command->data[2]) {
        // Return error on EVPD and other optional fields
        usb_storage_write_stream(0,0,1);
        usb_storage_scsi_respond_status(command->tag, 1,5,0x24,0);
      } else {
        usb_storage_write_stream(scsi_descriptor, MIN(36, allocation_length), 1);
        usb_storage_scsi_respond_status(command->tag, 0,0,0,0);
      }
      break;
    case 0x23:
      // READ FORMAT CAPACITIES (Windows requests this)
      usb_storage_write_stream(scsi_format_capacities, sizeof(scsi_format_capacities), 1);
      usb_storage_scsi_respond_status(command->tag, 0,0,0,0);
      break;
    case 0x25:
      // READ CAPACITY
      usb_storage_write_stream(scsi_capacity, sizeof(scsi_capacity), 1);
      usb_storage_scsi_respond_status(command->tag, 0,0,0,0);
      break;
    case 0x03:
      // REQUEST SENSE
      usb_storage_write_stream((char*)&sense_data, sizeof(sense_data), 1);
      usb_storage_scsi_respond_status(command->tag, 0,0,0,0);
      break;
    case 0x1a:
      // MODE SENSE
      usb_storage_write_stream(scsi_mode_sense, sizeof(scsi_mode_sense), 1);
      usb_storage_scsi_respond_status(command->tag, 0,0,0,0);
      break;
    case 0x28:
      // READ
      start_block = (command->data[2] << 24) | (command->data[3] << 16) | (command->data[4] << 8) | (command->data[5]);
      length = (command->data[7] << 8) | command->data[8];
      for(int block=start_block; block<start_block + length; block++) {
        char * sector_data;
        sector_data = floppy_read_sector(block);
        if(sector_data)
          usb_storage_write_stream(sector_data, 512, 0);
        else {
          usb_storage_write_stream(0,0,1);
          usb_storage_scsi_respond_status(command->tag, 1,3,0x11,1);
          return;
        }
      }
      //usb_storage_write_stream(0,0,1);
      usb_storage_scsi_respond_status(command->tag, 0,0,0,0);
      break;
    case 0x2A:
      // Write
      start_block = (command->data[2] << 24) | (command->data[3] << 16) | (command->data[4] << 8) | (command->data[5]);
      length = (command->data[7] << 8) | command->data[8];
      for(int block=start_block; block<start_block + length; block++) {
        char sector_data[512];
        for(int n=0; n<512; n+= 64)
          usb_read(1, sector_data + n);
        floppy_write_sector(block, sector_data);
      }
      usb_storage_scsi_respond_status(command->tag, 0,0,0,0);
      break;
    case 0x00: // CHECK UNIT READY
    case 0x1B: // START STOP UNIT
    case 0x2F: // VERIFY
      // All these commands can just return success
      usb_storage_scsi_respond_status(command->tag, 0,0,0,0);
      break;
    default:
      // UNKNOWN COMMAND
      usb_storage_scsi_respond_status(command->tag, 1, 5, 0x20, 0);
      break;
  }
}
