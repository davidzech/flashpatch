#pragma once

#include <gba/types.h>

#define FLASH_BASE ((u8 *)0xE000000)

#define FLASH_WRITE(addr, data) ((*(vu8 *)(FLASH_BASE + (addr))) = (data))

#define FLASH_ROM_SIZE_1M 131072 // 1 megabit ROM

#define SECTORS_PER_BANK 16

struct FlashSector {
    const u32 size;
    const u8 shift;
    const u16 count;
    const u16 top;
};

struct FlashType {
    const u32 romSize;
    const FlashSector sector;
    u16 wait[2];
    const union {
        struct {
            const u8 makerID;
            const u8 deviceID;
        } separate;
        const u16 joined;
    } ids;
};
