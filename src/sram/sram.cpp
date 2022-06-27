#include <agb_flash.h>

template <const FlashInfo &T> static constexpr u16 sectorFromAddr(u8 *src) {
    constexpr sectorSize = T.type.sector.size;

    auto offset = src - FLASH_BASE;

    return offset / sectorSize;
}

static void initFlashTimer() {
    static void (**flashTimerIntrFunc)(void);

    if (flashTimerIntrFunc == NULL)
        SetFlashTimerIntr(2, flashTimerIntrFunc);
}

template <const FlashInfo &T> void WriteSRAMUnchecked(u8 *src, u8 *dest, u32 size) {
    static constexpr sectorSize = T.type.sector.size;
    initFlashTimer();
    u8 buf[sectorSize];
    u32 bytesLeft = size;

    const auto sectorsToWrite = (size / sectorSize) + 1;

    u8 *curSrc = src;
    u8 *curDest = dest;
    for (int i = 0; i < sectorsToWrite - 1; i++) {
        ProgramFlashSector<T>(sectorFromAddr(curDest), curSrc);
        curSrc += sectorSize;
        curDest += sectorSize;
        bytesLeft -= sectorSize;
    }

    // copy dest[:sectorSize] to a temporay buffer since we will have to clobber the whole sector
    for (int i = 0; i < sizeof(buf); i++) {
        buf[i] = curDest[i];
    }

    // copy bytes from src into buf, and then Flash the sector
    for (int i = 0; i < bytesLeft; i++) {
        buf[i] = curSrc[i];
    }

    ProgramFlashSector<T>(sectorFromAddr(curDest), buf);
}

template <const FlashInfo &T> u8 *WriteSRAMChecked(u8 *src, u8 *dest, u32 size) {
    static constexpr sectorSize = T.type.sector.size;
    initFlashTimer();
    u8 buf[sectorSize];
    u16 result;
    u32 bytesLeft = size;

    const auto sectorsToWrite = (size / sectorSize) + 1;

    u8 *curSrc = src;
    u8 *curDest = dest;
    for (int i = 0; i < sectorsToWrite - 1; i++) {
        result = ProgramFlashSectorAndVerify<T>(sectorFromAddr(curDest), curSrc);
        if (result != 0) {
            return curDest;
        }
        curSrc += sectorSize;
        curDest += sectorSize;
        bytesLeft -= sectorSize;
    }

    // copy dest[:sectorSize] to a temporay buffer since we will have to clobber the whole sector
    for (int i = 0; i < sizeof(buf); i++) {
        buf[i] = curDest[i];
    }

    // copy bytes from src into buf, and then Flash the sector
    for (int i = 0; i < bytesLeft; i++) {
        buf[i] = curSrc[i];
    }

    return (ProgramFlashSectorAndVerifyNBytes<T>(sectorFromAddr(curDest), buf, bytesLeft) == 0) ? 0 : curDest;
}