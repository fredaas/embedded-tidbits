LD=arm-none-eabi-gcc
AS=arm-none-eabi-as
OBJCOPY=arm-none-eabi-objcopy

LDFLAGS=-nostdlib
ASFLAGS=-mcpu=cortex-m3 -mthumb -g

LINKERSCRIPT=efm32gg.ld

all : ex1_v1 ex1_v2

ex1_v2 : ex1_v2.elf
	${OBJCOPY} -j .text -O binary $< ex1_v2.bin

ex1_v1 : ex1_v1.elf
	${OBJCOPY} -j .text -O binary $< ex1_v1.bin

%.elf : %.o
	${LD} -T ${LINKERSCRIPT} $^ -o $@ ${LDFLAGS}

%.o : %.s
	${AS} ${ASFLAGS} $< -o $@

upload_v1 :
	-eACommander.sh -r --address 0x00000000 -f "ex1_v1.bin" -r

upload_v2 :
	-eACommander.sh -r --address 0x00000000 -f "ex1_v2.bin" -r

clean :
	-rm -rf *.o *.elf *.bin *.hex
