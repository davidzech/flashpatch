SHELL := /bin/bash
CC=clang
OBJCOPY=llvm-objcopy
ARCH=--target=arm-none-eabi -mthumb
CPPFLAGS=-std=c++20 $(ARCH) -O1 -fno-exceptions -nostdlib -nodefaultlibs -fno-builtin -I./ -frwpi -Wl,--entry=0
all:
	$(CC) $(CPPFLAGS) -S flashpatch.cpp
	$(CC) $(CPPFLAGS) -c flashpatch.cpp	
	$(CC) $(CPPFLAGS) flashpatch.cpp -o flashpatch.elf
	$(OBJCOPY) -j .text -O binary flashpatch.elf flashpatch.bin

.PHONY: clean

clean:
	rm *.bin *.elf *.o *.s