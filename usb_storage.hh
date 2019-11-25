char scsi_descriptor[] = {
  0x00, 0x80, 0x04, 0x02, 0x20,
  0,0,0,
  'c', 'a', 't', 'p', 'h', 'i', 's', 'h',
  'a', 'm', 'i', 'g', 'a', ' ', 'f', 'l',
  'o', 'p', 'p', 'y', ' ', ' ', ' ', ' ',
  '1', '.', '0', ' '
};

char scsi_capacity[] = {
  0,0,6,224, // 160 tracks * 11 sectors
  0,0,2,0    // 512 byte sectors
};
char scsi_mode_sense[] = {
  3,0,0,0
};
