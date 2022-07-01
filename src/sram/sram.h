#include <flash/flash.h>

template <const FlashInfo &T> static constexpr inline u16 sectorFromAddr(u8 *src) {
    constexpr auto sectorSize = T.type.sector.size;

    return (src - FLASH_BASE) / sectorSize;
}

// static void initFlashTimer() {
//     static void (**flashTimerIntrFunc)(void);

//     if (flashTimerIntrFunc == NULL)
//         SetFlashTimerIntr(2, flashTimerIntrFunc);
// }

template <const FlashInfo &T> void WriteSRAMUnchecked(u8 *src, u8 *dest, u32 size) {
    REG_WAITCNT = (REG_WAITCNT & ~WAITCNT_SRAM_MASK) | WAITCNT_SRAM_8;
    FLASH_WRITE(0x5555, 0xAA);
    FLASH_WRITE(0x2AAA, 0x55);
    FLASH_WRITE(0x5555, 0x90);

    FLASH_WRITE(0x5555, 0xAA);
    FLASH_WRITE(0x2AAA, 0x55);
    FLASH_WRITE(0x5555, 0xF0);
    constexpr auto sectorSize = T.type.sector.size;
    u8 buf[sectorSize];
    u32 bytesLeft = size;

    u8 *curSrc = src;
    u8 *curDest = dest;

    // determine sector our address clobbers, and how many bytes we can write to that sector (min(sectorEnd, bytesLeft))

    while (bytesLeft != 0) {
        u16 sectorNum = sectorFromAddr<T>(curDest);
        // read entire sector
        ReadFlash<T>(sectorNum, 0, buf, T.type.sector.size);
        uintptr_t sectorStart = (uintptr_t)(FLASH_BASE + (sectorNum << T.type.sector.shift));
        // translate the dest address into buf offset
        u32 offset = (uintptr_t)curDest - (uintptr_t)sectorStart;
        u8 *curBuf = buf + offset;
        // calculate how many bytes we can write to this sector,
        // at maximum we can only write sectorSize - offset.
        u32 bytesToWrite = bytesLeft < (sectorSize - offset) ? bytesLeft : (sectorSize - offset);
        for (u32 i = bytesToWrite; i != 0; i--) {
            *curBuf++ = *curSrc++;
        }
        // Flush buf back into Flash
        ProgramFlashSector<T>(sectorNum, buf);
        bytesLeft -= bytesToWrite;
    }
}

template <const FlashInfo &T> u8 *WriteSRAMChecked(u8 *src, u8 *dest, u32 size) {
    REG_WAITCNT = (REG_WAITCNT & ~WAITCNT_SRAM_MASK) | WAITCNT_SRAM_8;
    FLASH_WRITE(0x5555, 0xAA);
    FLASH_WRITE(0x2AAA, 0x55);
    FLASH_WRITE(0x5555, 0x90);

    FLASH_WRITE(0x5555, 0xAA);
    FLASH_WRITE(0x2AAA, 0x55);
    FLASH_WRITE(0x5555, 0xF0);
    constexpr auto sectorSize = T.type.sector.size;
    u8 buf[sectorSize];
    u32 bytesLeft = size;

    u8 *curSrc = src;
    u8 *curDest = dest;

    // determine sector our address clobbers, and how many bytes we can write to that sector (min(sectorEnd, bytesLeft))

    while (bytesLeft != 0) {
        u16 sectorNum = sectorFromAddr<T>(curDest);
        // read entire sector
        ReadFlash<T>(sectorNum, 0, buf, T.type.sector.size);
        uintptr_t sectorStart = (uintptr_t)(FLASH_BASE + (sectorNum << T.type.sector.shift));
        // translate the dest address into buf offset
        u32 offset = (uintptr_t)curDest - (uintptr_t)sectorStart;
        u8 *curBuf = buf + offset;
        // calculate how many bytes we can write to this sector,
        // at maximum we can only write sectorSize - offset.
        u32 bytesToWrite = bytesLeft < (sectorSize - offset) ? bytesLeft : (sectorSize - offset);
        for (u32 i = bytesToWrite; i != 0; i--) {
            *curBuf++ = *curSrc++;
        }
        // Flush buf back into Flash
        u8 *result = (u8 *)ProgramFlashSectorAndVerify<T>(sectorNum, buf);
        if (result != 0) {
            return result;
        }
        bytesLeft -= bytesToWrite;
    }

    return 0;
}