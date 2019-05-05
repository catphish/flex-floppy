# Amiga USB floppy controller

This is a USB floppy disk controller that aims to allow a modern PC to read and write data stored on Amiga formatted floppy disks using a small PCB inserted into the rear of a standard PC floppy drive. It supports 2 modes. The first (USB mass storage mode) allows easy reading and writing of data (including files or disk images) on standard formatted disks. The second mode allows precise copies of disks with non-standard formatting to be created.

## Amiga Mass storage

The device presents a mass storage device with a capacity of 901120 bytes and a block size of 5632 bytes (one track).

When a read is performed, the disk searches for the start of a track then decodes one full track of MFM as it is read from disk, storing it in SRAM (6250 bytes). Checksums are then verified, and the data portions of the track (512 * 11 = 5632 bytes) are found in order returned to the host. Metadata is discarded.

When a write is performed, the data from the host is written into a track structure in SRAM (5632 bytes of data are padded and spaced to 6250 bytes). The metadata and checksums are calculated and filled into the gaps. Finally this data is MFM coded as it is written to the disk.

The magnetic read/write does not occur at the same time as the USB data transfer so there are no timing constraints on the host or the USB channel.

## Raw Flux

This mode allows the host to read and write raw flux timings with 80MHz resolution. The device presents a serial interface that offers raw commands to select a track, then initiate a read or write. In this mode, the adapter does not process metadata, checksums, etc. and will work for any disk in any format.

When a read is performed, the disk starts reading at a random location and starts a 32-bit timer. Each time a flux transition (pulse from the drive) is detected, the the counter value from the previous pulse is subtracted from the current counter value. The difference is stored as a 16-bit value where 1 unit represents 1/80,000,000 of a second.

There are up to 50,000 magnetic transitions per track, read at a speed of 250,000 per second (0.2s per track). Encoded as 16-bit integers, this represents a storage requirement of 100,000 bytes, and a transfer speed of 500,000 bytes per second (4Mbps). This storage requirement, exceeds what is available in a low cost MCU, particularly as it is necessary to store more than just a single read of the track. Therefore in this mode, all transfers are dependent on streaming the data live over USB with adequate buffering.

In read mode, each transition time is appended to a ring buffer and transferred by USB as soon as possible. The read ends when 1 second (80million cycles) have elapsed. This provides a good level of redundancy for the host software to verify data.

In write mode, 16-bit integers are streamed from USB into a circular write buffer. When the buffer is full, writing starts. Pulses are send to the write heads of the drive at the timings specified.

The buffer size should be as large as availble SRAM permits.