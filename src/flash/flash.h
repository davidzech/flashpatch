#pragma once

/*
    Inspired from the Pokeruby decompilation project
*/

#include <gba/flash_internal.h>
#include <gba/gba.h>

constexpr u16 mxMaxTime[] = {
    10,   65469, TIMER_ENABLE | TIMER_INTR_ENABLE | TIMER_256CLK, 10,   65469, TIMER_ENABLE | TIMER_INTR_ENABLE | TIMER_256CLK,
    2000, 65469, TIMER_ENABLE | TIMER_INTR_ENABLE | TIMER_256CLK, 2000, 65469, TIMER_ENABLE | TIMER_INTR_ENABLE | TIMER_256CLK,
};

struct FlashInfo {
    const u16 *maxTime;
    const FlashType type;
};

// constexpr static u8 (*PollFlashStatus)(u8 *);
// static u8 sTimerNum;
// static u16 sTimerCount;
// static vu16 *sTimerReg;
// static u16 sSavedIme;
// static u8 sFlashTimeoutFlag = 0;

template <const FlashInfo &T> u16 WaitForFlashWrite(u8 phase, u8 *addr, u8 lastData);
u32 VerifyFlashSectorCore(u8 *src, u8 *tgt, u32 size);
u32 VerifyFlashSector(u16 sectorNum, u8 *src);
u32 VerifyFlashSectorNBytes(u16 sectorNum, u8 *src, u32 n);

template <const FlashInfo &T> void SwitchFlashBank(u16 sectorNum) {
    // not supported yet
    return;
}

void InitFlash() {
    REG_WAITCNT = (REG_WAITCNT & ~WAITCNT_SRAM_MASK) | WAITCNT_SRAM_8;
    FLASH_WRITE(0x5555, 0xAA);
    FLASH_WRITE(0x2AAA, 0x55);
    FLASH_WRITE(0x5555, 0x90);

    FLASH_WRITE(0x5555, 0xAA);
    FLASH_WRITE(0x2AAA, 0x55);
    FLASH_WRITE(0x5555, 0xF0);
}

__attribute__((noinline, target("thumb-mode"))) u8 ReadFlashByteCore(u8 *addr) { return *addr; }

__attribute__((noinline)) decltype(&ReadFlashByteCore) SetReadFlashByte(void *dest) {
    // ldrb r0, [r0]
    // bx lr

    auto fnDest = (u16 *)dest;

    fnDest[0] = 0x7800;
    fnDest[1] = 0x4770;

    // u16 *src;
    // u16 i;

    // u16 *funcSrc;
    // u16 *funcDest;

    // src = (u16 *)readFlashByteCoreASM;
    // // src = (u16 *)((uintptr_t)src ^ 1); // remove thumb offset from $PC

    // i = sizeof(readFlashByteCoreASM) / sizeof(*readFlashByteCoreASM);
    // funcDest = reinterpret_cast<u16 *>(dest);

    // while (i != 0) {
    //     *funcDest++ = *src++;
    //     i--;
    // }

    return decltype(&ReadFlashByteCore)((uintptr_t)fnDest + 1);
}

template <const FlashInfo &T> __attribute__((noinline)) u16 WaitForFlashWrite(u8 phase, u8 *addr, u8 lastData) {
    u16 result = 0;
    u16 delay = 0x2000;
    u16 buf[0x20];
    auto readFlashByte = SetReadFlashByte(buf);

    while (readFlashByte(addr) != lastData) {
        if (delay == 0) {
            result = 0xA000;
            break;
        }
        delay--;
    }

    return result;
}

u8 ReadFlashByte(u8 *addr) {
    u16 buf[0x20];
    return SetReadFlashByte(buf)(addr);
}

__attribute__((noinline, target("thumb-mode"))) void ReadFlashCore(u8 *src, u8 *dest, u32 size) {
    while (size-- != 0) {
        *dest++ = *src++;
    }
}

// __attribute__((noinline)) void SetReadFlash()

template <const FlashInfo &T> void ReadFlash(u16 sectorNum, u32 offset, u8 *dest, u32 size) {
    u8 *src;
    u16 i;
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

    u16 readFlashCoreBuffer[9];
    readFlashCoreBuffer[0] = 0x2a00;
    readFlashCoreBuffer[1] = 0xd005;
    readFlashCoreBuffer[2] = 0x7803;
    readFlashCoreBuffer[3] = 0x700b;
    readFlashCoreBuffer[4] = 0x1c49;
    readFlashCoreBuffer[5] = 0x1c40;
    readFlashCoreBuffer[6] = 0x1e52;
    readFlashCoreBuffer[7] = 0xd1f9;
    readFlashCoreBuffer[8] = 0x4770;

    u16 *funcSrc;
    u16 *funcDest;
    decltype(ReadFlashCore) *readFlashCore;

    REG_WAITCNT = (REG_WAITCNT & ~WAITCNT_SRAM_MASK) | WAITCNT_SRAM_8;

    if (T.type.romSize == FLASH_ROM_SIZE_1M) {
        SwitchFlashBank<T>(sectorNum / SECTORS_PER_BANK);
        sectorNum %= SECTORS_PER_BANK;
    }

    // funcSrc = (u16 *)readFlashCoreASM;
    // funcDest = readFlashCoreBuffer;

    // i = sizeof(readFlashCoreASM) / sizeof(*readFlashCoreASM);

    // while (i != 0) {
    //     *funcDest++ = *funcSrc++;
    //     i--;
    // }

    readFlashCore = decltype(readFlashCore)((uintptr_t)readFlashCoreBuffer + 1);

    src = FLASH_BASE + (sectorNum << T.type.sector.shift) + offset;

    readFlashCore(src, dest, size);
}

template <const FlashInfo &T> static u16 ProgramByte(u8 *src, u8 *dest) {
    // WAITCNT presumed set by caller

    FLASH_WRITE(0x5555, 0xAA);
    FLASH_WRITE(0x2AAA, 0x55);
    FLASH_WRITE(0x5555, 0xA0);
    *dest = *src;

    return WaitForFlashWrite<T>(1, dest, *src);
}

template <const FlashInfo &T> u16 EraseFlashSector(u16 sectorNum) {
    u16 numTries = 0;
    u16 result;
    u8 *addr;
    u16 readFlash1Buffer[0x20];

    if (sectorNum >= T.type.sector.count) {
        return 0x80FF;
    }

    SwitchFlashBank<T>(sectorNum / SECTORS_PER_BANK);
    sectorNum %= SECTORS_PER_BANK;

try_erase:
    REG_WAITCNT = (REG_WAITCNT & ~WAITCNT_SRAM_MASK) | T.type.wait[0];

    addr = FLASH_BASE + (sectorNum << T.type.sector.shift);

    FLASH_WRITE(0x5555, 0xAA);
    FLASH_WRITE(0x2AAA, 0x55);
    FLASH_WRITE(0x5555, 0x80);
    FLASH_WRITE(0x5555, 0xAA);
    FLASH_WRITE(0x2AAA, 0x55);
    *addr = 0x30;

    // SetReadFlash1((u8 *)readFlash1Buffer);

    result = WaitForFlashWrite<T>(2, addr, 0xFF);

    if (!(result & 0xA000) || numTries > 3) {
        goto done;
    }

    numTries++;

    goto try_erase;

done:
    REG_WAITCNT = (REG_WAITCNT & ~WAITCNT_SRAM_MASK) | WAITCNT_SRAM_8;

    return result;
}

template <const FlashInfo &T> u16 ProgramFlashSector(u16 sectorNum, u8 *src) {
    u8 *dest;
    // u16 readFlash1Buffer[0x20];

    if (sectorNum >= T.type.sector.count) {
        return 0x80FF;
    }

    u16 result = EraseFlashSector<T>(sectorNum);

    if (result != 0) {
        return result;
    }

    SwitchFlashBank<T>(sectorNum / SECTORS_PER_BANK);
    sectorNum %= SECTORS_PER_BANK;

    // SetReadFlash1((u8 *)readFlash1Buffer);

    REG_WAITCNT = (REG_WAITCNT & ~WAITCNT_SRAM_MASK) | T.type.wait[0];

    u16 flashNumRemainingBytes = T.type.sector.size;
    dest = FLASH_BASE + (sectorNum << T.type.sector.shift);

    while (flashNumRemainingBytes > 0) {
        result = ProgramByte<T>(src, dest);

        if (result != 0) {
            break;
        }

        flashNumRemainingBytes--;
        src++;
        dest++;
    }

    return 0;
}

template <const FlashInfo &T> u32 ProgramFlashSectorAndVerify(u16 sectorNum, u8 *src) {
    u32 result;

    for (u8 i = 0; i < 3; i++) {
        result = ProgramFlashSector<T>(sectorNum, src);
        if (result != 0) {
            continue;
        }

        result = VerifyFlashSector(sectorNum, src);
        if (result == 0) {
            break;
        }
    }

    return result;
}

template <const FlashInfo &T> u32 ProgramFlashSectorAndVerifyNBytes(u16 sectorNum, u8 *dataSrc, u32 n) {
    u32 result;

    for (u8 i = 0; i < 3; i++) {
        result = ProgramFlashSector<T>(sectorNum, dataSrc);
        if (result != 0) {
            continue;
        }

        result = VerifyFlashSectorNBytes(sectorNum, dataSrc, n);
        if (result != 0) {
            continue;
        }
    }

    return result;
}

// void FlashTimerIntr() {
//     if (sTimerCount != 0 && --sTimerCount == 0) {
//         sFlashTimeoutFlag = 1;
//     }
// }

// u16 SetFlashTimerIntr(u8 timerNum, decltype(FlashTimerIntr) **intrFunc) {
//     if (timerNum >= 4) {
//         return 1;
//     }

//     sTimerNum = timerNum;
//     sTimerReg = (vu16 *)((uintptr_t)REG_ADDR_TMCNT + (timerNum * 4));
//     *intrFunc = FlashTimerIntr;
//     return 0;
// }

// template <const FlashInfo &T> void StartFlashTimer(u8 phase) {
//     const u16 *maxTimes = &T.maxTime[phase * 3];
//     sSavedIme = REG_IME;
//     REG_IME = 0;
//     sTimerReg[1] = 0;
//     REG_IE = REG_IE | (INTR_FLAG_TIMER0 << sTimerNum);
//     sFlashTimeoutFlag = 0;
//     sTimerCount = *maxTimes++;
//     *sTimerReg++ = *maxTimes++;
//     *sTimerReg-- = *maxTimes++;
//     REG_IF = (INTR_FLAG_TIMER0 << sTimerNum);
//     REG_IME = 1;
// }

// void StopFlashTimer() {
//     REG_IME = 0;
//     *sTimerReg++ = 0;
//     *sTimerReg-- = 0;
// }

template <const FlashInfo &T> u16 EraseFlashChip() {
    u16 result;

    REG_WAITCNT = (REG_WAITCNT & ~WAITCNT_SRAM_MASK) | T.type.wait[0];

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

    result = WaitForFlashWrite<T>(3, FLASH_BASE, 0xFF);

    REG_WAITCNT = (REG_WAITCNT & ~WAITCNT_SRAM_MASK) | WAITCNT_SRAM_8;

    return result;
};

u32 VerifyFlashSectorCore(u8 *src, u8 *tgt, u32 size) { return 0; }

u32 VerifyFlashSector(u16 sectorNum, u8 *src) { return 0; }

u32 VerifyFlashSectorNBytes(u16 sectorNum, u8 *src, u32 n) { return 0; }