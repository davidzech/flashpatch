SHELL := /bin/bash
CC=clang
OBJCOPY=llvm-objcopy
ARCH=--target=arm-none-eabi
CPPFLAGS=-std=c++20 $(ARCH) -O1  -Wall -fno-exceptions -nostdlib -nodefaultlibs -fno-builtin -I./ -frwpi -Wl,--entry=0
all:
	$(CC) $(CPPFLAGS) -S flashpatch.cpp
	$(CC) $(CPPFLAGS) -c flashpatch.cpp	
	$(CC) $(CPPFLAGS) -gsplit-dwarf -g flashpatch.cpp -o flashpatch.elf
	$(OBJCOPY) -j .text -O binary flashpatch.elf flashpatch.bin

.PHONY: clean

clean:
	rm *.bin *.elf *.o *.s
