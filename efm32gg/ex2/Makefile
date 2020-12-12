CC=arm-none-eabi-gcc
LD=arm-none-eabi-gcc
OBJCOPY=arm-none-eabi-objcopy

CFLAGS=-mcpu=cortex-m3 -mthumb -g -std=c99 -Wall
LDFLAGS=-mcpu=cortex-m3 -mthumb -g -lgcc -lc -lcs3 -lcs3unhosted -lefm32gg -Llib
ASFLAGS=-mcpu=cortex-m3 -mthumb -g
LINKERSCRIPT=lib/efm32gg.ld

all : clean ex2_v1.bin ex2_v2.bin

ex2_v1.bin : ex2_v1.elf
	${OBJCOPY} -O binary $< $@
ex2_v1.elf : ex2_v1.o
	${LD} -T ${LINKERSCRIPT} $^ -o $@ ${LDFLAGS}

ex2_v2.bin : ex2_v2.elf
	${OBJCOPY} -O binary $< $@
ex2_v2.elf : ex2_v2.o
	${LD} -T ${LINKERSCRIPT} $^ -o $@ ${LDFLAGS}

%.o : %.c
	${CC} ${CFLAGS} -c $< -o $@

upload_v1 :
	-eACommander.sh -r --address 0x00000000 -f "ex2_v1.bin" -r

upload_v2 :
	-eACommander.sh -r --address 0x00000000 -f "ex2_v2.bin" -r

clean :
	-rm -rf *.o *.elf *.bin *.hex
