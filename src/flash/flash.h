#pragma once

/*
    Inspired from the Pokeruby decompilation project
*/

#include <gba/flash_internal.h>
#include <gba/gba.h>

namespace Flash {
constexpr u16 mxMaxTime[] = {
    10,   65469, TIMER_ENABLE | TIMER_INTR_ENABLE | TIMER_256CLK, 10,   65469, TIMER_ENABLE | TIMER_INTR_ENABLE | TIMER_256CLK,
    2000, 65469, TIMER_ENABLE | TIMER_INTR_ENABLE | TIMER_256CLK, 2000, 65469, TIMER_ENABLE | TIMER_INTR_ENABLE | TIMER_256CLK,
};

using Type = FlashType;

struct Info {
    const u16 *const maxTime;
    const Type type;
};

constexpr Info MX29L010 = {.maxTime = mxMaxTime,
                           .type = {
                               .romSize = 131072,
                               .sector =
                                   {
                                       .size = 4096,
                                       .shift = 12,
                                       .count = 32,
                                       .top = 0,
                                   },
                           }};

constexpr Info SST39SF512 = {.maxTime = mxMaxTime,
                             .type = {.romSize = 64 * 1024,
                                      .sector = {
                                          .size = 4096,
                                          .shift = 12,
                                          .count = 16,
                                          .top = 0,
                                      }}};

template <const Info &F = SST39SF512> class Chip {
  private:
    // Never actually used.
    __attribute__((noinline, target("thumb-mode"))) static u8 ReadByteCore(u8 *addr) { return *addr; }

    __attribute__((noinline, target("thumb-mode"))) static void ReadFlashCore(u8 *src, u8 *dest, u32 size) {
        while (size-- != 0) {
            *dest++ = *src++;
        }
    }

  public:
    Chip() = delete;

    static constexpr auto Info = F;

    static void SwitchBank(u16 sectorNum) {
        // not supported yet
        return;
    }

    static void Init() {
        REG_WAITCNT = (REG_WAITCNT & ~WAITCNT_SRAM_MASK) | WAITCNT_SRAM_8;
        FLASH_WRITE(0x5555, 0xAA);
        FLASH_WRITE(0x2AAA, 0x55);
        FLASH_WRITE(0x5555, 0x90);

        FLASH_WRITE(0x5555, 0xAA);
        FLASH_WRITE(0x2AAA, 0x55);
        FLASH_WRITE(0x5555, 0xF0);
    }

    struct ReadByteFunc {
      private:
        u16 Code[2];

      public:
        ReadByteFunc() {
            // ldrb r0, [r0]
            // bx lr
            Code[0] = 0x7800;
            Code[1] = 0x4770;
        }

        u8 operator()(u8 *addr) {
            auto func = decltype(&ReadByteCore)((uintptr_t)Code + 1);
            return func(addr);
        }
    };

    static u16 Wait(u8 phase, u8 *addr, u8 lastData) {
        u16 result = 0;
        u16 delay = 2000;
        ReadByteFunc readFlashByte;

        while (readFlashByte(addr) != lastData) {
            if (delay == 0) {
                result = 0xA000;
                break;
            }
            delay--;
        }

        return result;
    }

    static u8 ReadByte(u8 *addr) {
        ReadByteFunc buf;
        return buf(addr);
    }

    struct ReadCoreFunc {
        u16 Code[9];

        ReadCoreFunc() {
            /*
            000000e0  002a       cmp     r2, #0
            000000e2  05d0       beq     #0xf0

            000000e4  0378       ldrb    r3, [r0]
            000000e6  0b70       strb    r3, [r1]
            000000e8  491c       adds    r1, r1, #1
            000000ea  401c       adds    r0, r0, #1
            000000ec  521e       subs    r2, r2, #1
            000000ee  f9d1       bne     #0xe4

            000000f0  7047       bx      lr
            */
            Code[0] = 0x2a00;
            Code[1] = 0xd005;
            Code[2] = 0x7803;
            Code[3] = 0x700b;
            Code[4] = 0x1c49;
            Code[5] = 0x1c40;
            Code[6] = 0x1e52;
            Code[7] = 0xd1f9;
            Code[8] = 0x4770;
        }

        void operator()(u8 *dest, u8 *src, u32 size) const {
            auto func = decltype(&ReadFlashCore)((uintptr_t)Code + 1);
            return func(dest, src, size);
        }
    };

    static void ReadSector(u16 sectorNum, u32 offset, u8 *dest, u32 size) {
        u8 *src;
        ReadCoreFunc readFlashCore;

        REG_WAITCNT = (REG_WAITCNT & ~WAITCNT_SRAM_MASK) | WAITCNT_SRAM_8;

        if (F.type.romSize == FLASH_ROM_SIZE_1M) {
            SwitchBank(sectorNum / SECTORS_PER_BANK);
            sectorNum %= SECTORS_PER_BANK;
        }

        src = FLASH_BASE + (sectorNum << F.type.sector.shift) + offset;

        readFlashCore(dest, src, size);
    }

    static void ReadMem(u8 *dest, u8 *src, u32 size) {
        ReadCoreFunc readFlashCore;

        REG_WAITCNT = (REG_WAITCNT & ~WAITCNT_SRAM_MASK) | WAITCNT_SRAM_8;

        if (F.type.romSize == FLASH_ROM_SIZE_1M) {
            // determine sectorNum from src
            // SwitchBank(sectorNum / SECTORS_PER_BANK);
            // sectorNum %= SECTORS_PER_BANK;
        }

        readFlashCore(dest, src, size);
    }

    static u16 WriteByte(u8 *dest, u8 *src, bool wait = true) {
        REG_WAITCNT = (REG_WAITCNT & ~WAITCNT_SRAM_MASK) | F.type.wait[0];

        FLASH_WRITE(0x5555, 0xAA);
        FLASH_WRITE(0x2AAA, 0x55);
        FLASH_WRITE(0x5555, 0xA0);
        *dest = *src;

        u16 result = 0;
        if (wait)
            result = Wait(1, dest, *src);

        REG_WAITCNT = (REG_WAITCNT & ~WAITCNT_SRAM_MASK) | WAITCNT_SRAM_8;
        return result;
    }

    static u16 WriteByte(u8 *dest, u8 b, bool wait = true) {
        REG_WAITCNT = (REG_WAITCNT & ~WAITCNT_SRAM_MASK) | F.type.wait[0];

        FLASH_WRITE(0x5555, 0xAA);
        FLASH_WRITE(0x2AAA, 0x55);
        FLASH_WRITE(0x5555, 0xA0);
        dest[0] = (u8)b;

        u16 result = 0;
        if (wait)
            result = Wait(1, dest, b);

        REG_WAITCNT = (REG_WAITCNT & ~WAITCNT_SRAM_MASK) | WAITCNT_SRAM_8;
        return result;
    }

    static u16 EraseSector(u16 sectorNum, bool wait = true) {
        constexpr int numTries = 3;
        u16 result;
        u8 *addr;

        if (sectorNum >= F.type.sector.count) {
            return 0x80FF;
        }

        SwitchBank(sectorNum / SECTORS_PER_BANK);
        sectorNum %= SECTORS_PER_BANK;

        for (int i = 0; i < numTries; i++) {
            REG_WAITCNT = (REG_WAITCNT & ~WAITCNT_SRAM_MASK) | F.type.wait[0];

            addr = FLASH_BASE + (sectorNum << F.type.sector.shift);

            FLASH_WRITE(0x5555, 0xAA);
            FLASH_WRITE(0x2AAA, 0x55);
            FLASH_WRITE(0x5555, 0x80);
            FLASH_WRITE(0x5555, 0xAA);
            FLASH_WRITE(0x2AAA, 0x55);
            *addr = 0x30;

            if (wait) {
                result = Wait(2, addr, 0xFF);

                if (result == 0) {
                    break;
                }
            }
        }

        REG_WAITCNT = (REG_WAITCNT & ~WAITCNT_SRAM_MASK) | WAITCNT_SRAM_8;

        return result;
    }

    static u16 WriteSector(u16 sectorNum, u8 src[F.type.sector.size]) {
        u8 *dest;

        if (sectorNum >= F.type.sector.count) {
            return 0x80FF;
        }

        u16 result = EraseSector(sectorNum);

        if (result != 0) {
            return result;
        }

        SwitchBank(sectorNum / SECTORS_PER_BANK);
        sectorNum %= SECTORS_PER_BANK;

        // SetReadFlash1((u8 *)readFlash1Buffer);

        REG_WAITCNT = (REG_WAITCNT & ~WAITCNT_SRAM_MASK) | F.type.wait[0];

        u16 flashNumRemainingBytes = F.type.sector.size;
        dest = FLASH_BASE + (sectorNum << F.type.sector.shift);

        while (flashNumRemainingBytes > 0) {
            result = WriteByte(dest, src);

            if (result != 0) {
                break;
            }

            flashNumRemainingBytes--;
            src++;
            dest++;
        }

        REG_WAITCNT = (REG_WAITCNT & ~WAITCNT_SRAM_MASK) | WAITCNT_SRAM_8;

        return 0;
    }

    static u32 WriteSectorVerify(u16 sectorNum, u8 src[F.type.sector.size]) {
        u32 result;

        for (u8 i = 0; i < 3; i++) {
            result = WriteSector(sectorNum, src);
            if (result != 0) {
                continue;
            }

            result = VerifySector(sectorNum, src);
            if (result == 0) {
                break;
            }
        }

        return result;
    }

    static u16 EraseChip() {
        u16 result;

        REG_WAITCNT = (REG_WAITCNT & ~WAITCNT_SRAM_MASK) | F.type.wait[0];

        // initiate command
        FLASH_WRITE(0x5555, 0xAA);
        FLASH_WRITE(0x2AAA, 0x55);
        // prepare erase
        FLASH_WRITE(0x5555, 0x80);
        // initiate command
        FLASH_WRITE(0x5555, 0xAA);
        FLASH_WRITE(0x2AAA, 0x55);
        // erase chip
        FLASH_WRITE(0x5555, 0x10);

        result = Wait(3, FLASH_BASE, 0xFF);

        REG_WAITCNT = (REG_WAITCNT & ~WAITCNT_SRAM_MASK) | WAITCNT_SRAM_8;

        return result;
    };

    static u32 VerifySector(u16 sectorNum, u8 *src) { return 0; }

    template <class T> static u16 Write(const T &v, const T *dest) {
        u8 *buf = (u8 *)&v;
        u8 *dst = (u8 *)dest;
        for (int i = 0; i < sizeof(T); i++) {
            auto result = WriteByte(&dst[i], buf[i]);
            if (result != 0) {
                return result;
            }
        }
        return 0;
    }

    template <class T> static T Read(const T *const src, ReadByteFunc &readByte) {
        REG_WAITCNT = (REG_WAITCNT & ~WAITCNT_SRAM_MASK) | WAITCNT_SRAM_8;

        T out;
        u8 *buf = (u8 *)&out;
        u8 *addr = (u8 *)src;
        for (int i = 0; i < sizeof(T); i++) {
            buf[i] = readByte(&addr[i]);
        }

        return out;
    }

    template <class T> static T Read(const T *const src) {
        ReadByteFunc func;
        return Read(src, func);
    }
};
} // namespace Flash