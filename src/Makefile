CC=clang
ARCH=--target=arm-none-eabi -mthumb
CPPFLAGS=-std=c++20 $(ARCH) -O0 -fno-exceptions -I./

all:
	$(CC) $(CPPFLAGS) -S flashpatch.cpp
	$(CC) $(ARCH) -c flashpatch.s