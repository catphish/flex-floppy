set -e
rm -f *.o
PATH=/home/charlie/gcc-arm-none-eabi-8-2018-q4-major/bin:/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin:/usr/games:/usr/local/games:/snap/bin:/home/charlie/.rvm/bin:/home/charlie/.rvm/bin
REPOROOT="/home/charlie/Applications/STM32Cube_FW_L4_V1.14.0"
CCOPTS="-Wall -mcpu=cortex-m4 -mthumb -mfloat-abi=hard -mfpu=fpv4-sp-d16 -I$REPOROOT/Drivers/CMSIS/Device/ST/STM32L4xx/Include -I$REPOROOT/Drivers/CMSIS/Include -DSTM32L4xx -O0 -ffast-math"
arm-none-eabi-gcc $CCOPTS -c startup_stm32l433xx.s -o startup_stm32l433xx.o
arm-none-eabi-gcc $CCOPTS -c system.c -o system.o
arm-none-eabi-gcc $CCOPTS -c main.c -o main.o
arm-none-eabi-gcc $CCOPTS -c util.c -o util.o
arm-none-eabi-gcc $CCOPTS -c usb.c -o usb.o
arm-none-eabi-gcc $CCOPTS -c floppy.c -o floppy.o
arm-none-eabi-gcc $CCOPTS -c usb_storage.c -o usb_storage.o
arm-none-eabi-gcc $CCOPTS -c gpio.c -o gpio.o
arm-none-eabi-gcc $CCOPTS -T STM32L433RCTx_FLASH.ld -Wl,--gc-sections *.o -o main.elf -lm
arm-none-eabi-objcopy -O binary main.elf main.bin
rm *.o
st-flash write main.bin 0x8000000
