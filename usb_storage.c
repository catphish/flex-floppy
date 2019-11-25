#include <stm32l433xx.h>
#include <stdint.h>
#include "usb_storage.h"
#include "usb_storage.hh"
#include "usb.h"
#include "util.h"

char usb_storage_write_stream_buffer[64];
uint8_t usb_storage_write_stream_buffer_pos = 0;
struct sense_data sense_data;

void usb_storage_write_stream(volatile char * buffer, uint32_t len) {
  // Copy buffer to usb_write_stream_buffer
  for(int n=0; n<len; n++) {
    usb_storage_write_stream_buffer[usb_storage_write_stream_buffer_pos] = buffer[n];
    usb_storage_write_stream_buffer_pos++;
    // Transmit when we have 64 bytes in the buffer
    if(usb_storage_write_stream_buffer_pos == 64) {
      usb_write(0x81, usb_storage_write_stream_buffer, 64);
      usb_storage_write_stream_buffer_pos = 0;
    }
  }
}

void usb_storage_terminate_stream() {
  // Transmit whatever is left in the buffer
  // This may be an empty packet, that's still the correct way to terminate
  usb_write(0x81, usb_storage_write_stream_buffer, usb_storage_write_stream_buffer_pos);
  usb_storage_write_stream_buffer_pos = 0;
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
  usb_storage_write_stream((char*)&csw, sizeof(csw));
  usb_storage_terminate_stream();
}

void usb_storage_handle_ep1() {
  char packet[64];
  usb_read(1, packet);
  struct command_block_wrapper * command = (struct command_block_wrapper *)&packet;
  uint16_t allocation_length;
  uint32_t length;

  switch(command->data[0]) {
    case 0x12:
      allocation_length = command->data[4];
      // INQUIRY
      usb_storage_write_stream(scsi_descriptor, MIN(36, allocation_length));
      usb_storage_terminate_stream();
      usb_storage_scsi_respond_status(command->tag, 0,0,0,0);
      break;
    case 0x25:
      // READ CAPACITY
      usb_storage_write_stream(scsi_capacity, sizeof(scsi_capacity));
      usb_storage_terminate_stream();
      usb_storage_scsi_respond_status(command->tag, 0,0,0,0);
      break;
    case 0x03:
      // REQUEST SENSE
      usb_storage_write_stream((char*)&sense_data, sizeof(sense_data));
      usb_storage_terminate_stream();
      usb_storage_scsi_respond_status(command->tag, 0,0,0,0);
      break;
    case 0x1a:
      // MODE SENSE
      usb_storage_write_stream(scsi_mode_sense, sizeof(scsi_mode_sense));
      usb_storage_terminate_stream();
      usb_storage_scsi_respond_status(command->tag, 0,0,0,0);
      break;
    case 0x28:
      // READ
      length = (command->data[7] << 8) | command->data[8];
      length *= 512;
      for(uint32_t n=0;n<length;n++)
        usb_storage_write_stream("\1", 1);
      usb_storage_terminate_stream();
      usb_storage_scsi_respond_status(command->tag, 0,0,0,0);
      break;
    case 0x00: // CHECK UNIT READY
    case 0x1B: // START STOP UNIT
    case 0x1E: // PREVENT ALLOW MEDIUM REMOVAL
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
