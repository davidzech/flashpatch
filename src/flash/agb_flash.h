#pragma once

/*
    Inspired from the Pokeruby decompilation project
*/

#include <gba/flash_internal.h>
#include <gba/gba.h>

const u16 mxMaxTime[] = {
    10,   65469, TIMER_ENABLE | TIMER_INTR_ENABLE | TIMER_256CLK, 10,   65469, TIMER_ENABLE | TIMER_INTR_ENABLE | TIMER_256CLK,
    2000, 65469, TIMER_ENABLE | TIMER_INTR_ENABLE | TIMER_256CLK, 2000, 65469, TIMER_ENABLE | TIMER_INTR_ENABLE | TIMER_256CLK,
};

struct FlashInfo {
    const u16 *maxTime;
    const FlashType type;
};

static u8 (*PollFlashStatus)(u8 *);
static u8 sTimerNum;
static u16 sTimerCount;
static vu16 *sTimerReg;
static u16 sSavedIme;
static u8 sFlashTimeoutFlag = 0;

template <const FlashInfo &T> void SwitchFlashbank(u16 sectorNum) {
    // not supported yet
    return;
}

u8 ReadFlash1(u8 *addr) { return *addr; }

void SetReadFlash1(u8 *dest) {
    u16 *src;
    u16 i;

    PollFlashStatus = decltype(PollFlashStatus)((u8 *)dest + 1);

    src = (u16 *)ReadFlash1;
    src = (u16 *)((uintptr_t)src ^ 1); // handle thumb

    i = ((u8 *)SetReadFlash1 - (u8 *)ReadFlash1) >> 1;

    while (i != 0) {
        *dest++ = *src++;
        i--;
    }
}

void ReadFlashCore(u8 *src, u8 *dest, u32 size) {
    while (size-- != 0) {
        *dest++ = *src++;
    }
}

void _ReadFlashCoreEND(u16 sectorNum, u32 offset, u8 *dest, u32 size) {}

template <const FlashInfo &T> void ReadFlash(u16 sectorNum, u32 offset, u8 *dest, u32 size) {
    u8 *src;
    u16 i;
    vu16 readFlashCoreBuffer[0x40];
    vu16 *funcSrc;
    vu16 *funcDest;
    decltype(ReadFlashCore) *readFlashCore;

    static_assert(sizeof(readFlashCoreBuffer) >= uintptr_t(_ReadFlashCoreEND) - uintptr_t(ReadFlashCore), "ReadFlashCore must fit into buffer");

    REG_WAITCNT = (REG_WAITCNT & ~WAITCNT_SRAM_MASK) | WAITCNT_SRAM_8;

    if (T.type.romSize == FLASH_ROM_SIZE_1M) {
        SwitchFlashbank<T>(sectorNum / SECTORS_PER_BANK);
        sectorNum %= SECTORS_PER_BANK;
    }

    funcSrc = (vu16 *)ReadFlashCore;
    funcSrc = (vu16 *)((uintptr_t)funcSrc ^ 1);
    funcDest = readFlashCoreBuffer;

    i = ((uintptr_t)_ReadFlashCoreEND - (uintptr_t)ReadFlashCore) >> 1;

    while (i != 0) {
        *funcDest++ = *funcSrc++;
        i--;
    }

    readFlashCore = decltype(readFlashCore)((uintptr_t)readFlashCoreBuffer + 1);

    src = FLASH_BASE + (sectorNum << T.type.sector.shift) + offset;

    readFlashCore(src, dest, size);
}

u32 VerifyFlashSectorCore(u8 *src, u8 *tgt, u32 size) { return 0; }

u32 VerifyFlashSector(u16 sectorNum, u8 *src) { return 0; }

u32 VerifyFlashSectorNBytes(u16 sectorNum, u8 *src, u32 n) { return 0; }

template <const FlashInfo &T> static u16 ProgramByte(u8 *src, u8 *dest) {

    FLASH_WRITE(0x5555, 0xAA);
    FLASH_WRITE(0x2AAA, 0x55);
    FLASH_WRITE(0x5555, 0xA0);
    *dest = *src;

    return WaitForFlashWrite<T>(1, dest, *src);
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

    SwitchFlashbank<T>(sectorNum / SECTORS_PER_BANK);
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

        result = VerifyFlashSector<T>(sectorNum, src);
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

        result = VerifyFlashSectorNBytes<T>(sectorNum, dataSrc, n);
        if (result != 0) {
            continue;
        }
    }

    return result;
}

void FlashTimerIntr() {
    if (sTimerCount != 0 && --sTimerCount == 0) {
        sFlashTimeoutFlag = 1;
    }
}

u16 SetFlashTimerIntr(u8 timerNum, decltype(FlashTimerIntr) **intrFunc) {

    if (timerNum >= 4) {
        return 1;
    }

    sTimerNum = timerNum;
    sTimerReg = (vu16 *)((uintptr_t)REG_ADDR_TMCNT + (timerNum * 4));
    *intrFunc = FlashTimerIntr;
    return 0;
}

template <const FlashInfo &T> void StartFlashTimer(u8 phase) {
    const u16 *maxTimes = &T.maxTime[phase * 3];
    sSavedIme = REG_IME;
    REG_IME = 0;
    sTimerReg[1] = 0;
    REG_IE = REG_IE | (INTR_FLAG_TIMER0 << sTimerNum);
    sFlashTimeoutFlag = 0;
    sTimerCount = *maxTimes++;
    *sTimerReg++ = *maxTimes++;
    *sTimerReg-- = *maxTimes++;
    REG_IF = (INTR_FLAG_TIMER0 << sTimerNum);
    REG_IME = 1;
}

void StopFlashTimer() {
    REG_IME = 0;
    *sTimerReg++ = 0;
    *sTimerReg-- = 0;
}

template <const FlashInfo &T> u16 WaitForFlashWrite(u8 phase, u8 *addr, u8 lastData) {
    u16 result = 0;
    u8 status;

    StartFlashTimer<T>(phase);

    while ((status = PollFlashStatus(addr)) != lastData) {
        if (status & 0x20) {
            // Write op exceeded time limit)

            if (PollFlashStatus(addr) == lastData) {
                break;
            }

            FLASH_WRITE(0x5555, 0xF0);
            result = phase | 0xA000u;
            break;
        }

        if (sFlashTimeoutFlag) {
            if (PollFlashStatus(addr) == lastData) {
                break;
            }

            FLASH_WRITE(0x5555, 0xF0);
            result = phase | 0xC000u;
            break;
        }
    }

    StopFlashTimer();

    return result;
}

template <const FlashInfo &T> u16 EraseFlashChip() {
    u16 result;
    u16 readFlash1Buffer[0x20];

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

    SetReadFlash1((u8 *)readFlash1Buffer); // sets PollFlashStatus... gross code
    result = WaitForFlashWrite<T>(3, FLASH_BASE, 0xFF);

    REG_WAITCNT = (REG_WAITCNT & ~WAITCNT_SRAM_MASK) | WAITCNT_SRAM_8;

    return result;
};

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

    SetReadFlash1((u8 *)readFlash1Buffer);

    result = WaitForFlashWrite<T>(2, addr, 0xFF);

    if (!(result & 0xA) || numTries > 3) {
        goto done;
    }

    numTries++;

    goto try_erase;

done:
    REG_WAITCNT = (REG_WAITCNT & ~WAITCNT_SRAM_MASK) | WAITCNT_SRAM_8;

    return result;
}