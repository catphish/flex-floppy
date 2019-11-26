struct command_block_wrapper {
  uint32_t signature;
  uint32_t tag;
  uint32_t data_transfer_length;
  uint8_t  flags;
  uint8_t  lun;
  uint8_t  cdb_length;

  uint8_t  data[16];
} __attribute__((packed));

struct command_status_wrapper {
  uint32_t signature;
  uint32_t tag;
  uint32_t data_residue;
  uint8_t  status;
} __attribute__((packed));

struct sense_data {
  uint8_t  response_code;
  uint8_t  segment_number;

  unsigned sense_key  : 4;
  unsigned reserved   : 1;
  unsigned ili        : 1;
  unsigned eom        : 1;
  unsigned file_mark  : 1;

  uint8_t  information[4];
  uint8_t  additional_length;
  uint8_t  command_specific_information[4];
  uint8_t  additional_sense_code;
  uint8_t  additional_sense_code_qualifier;
  uint8_t  field_replaceable_unit_code;
  uint8_t  sense_key_specific[3];
} __attribute__((packed));

void usb_storage_write_stream(char * buffer, uint32_t len, uint8_t terminate);
void usb_storage_scsi_respond_status(uint32_t tag, uint8_t status, uint8_t sense_key, uint8_t additional_sense_code, uint8_t additional_sense_code_qualifier);
void usb_storage_handle_ep1();
