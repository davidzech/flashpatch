#include <flash/flash.h>

template <const FlashInfo &T> static constexpr inline u16 sectorFromAddr(u8 *src) {
    constexpr auto sectorSize = T.type.sector.size;

    return (src - FLASH_BASE) / sectorSize;
}

decltype(&ReadFlashCore) SetMemCopy(void *dest) {
    auto buf = (u16 *)dest;
    buf[0] = 0x2a00;
    buf[1] = 0xd005;
    buf[2] = 0x7803;
    buf[3] = 0x700b;
    buf[4] = 0x1c49;
    buf[5] = 0x1c40;
    buf[6] = 0x1e52;
    buf[7] = 0xd1f9;
    buf[8] = 0x4770;

    return decltype(&ReadFlashCore)((uintptr_t)buf + 1);
}

void MemCopy(u8 *src, u8 *dest, u32 size) {
    u16 buf[9];
    SetMemCopy(buf)(src, dest, size);
}

template <const FlashInfo &T> void WriteSRAMUnchecked(u8 *src, u8 *dest, u32 size) {
    REG_WAITCNT = (REG_WAITCNT & ~WAITCNT_SRAM_MASK) | WAITCNT_SRAM_8;
    FLASH_WRITE(0x5555, 0xAA);
    FLASH_WRITE(0x2AAA, 0x55);
    FLASH_WRITE(0x5555, 0x90);

    FLASH_WRITE(0x5555, 0xAA);
    FLASH_WRITE(0x2AAA, 0x55);
    FLASH_WRITE(0x5555, 0xF0);

    auto destRegion = reinterpret_cast<uintptr_t>(dest) >> 24;

    if (destRegion == 0xE) {

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
    } else {
        MemCopy(src, dest, size);
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

    auto destRegion = reinterpret_cast<uintptr_t>(dest) >> 24;

    if (destRegion == 0xE) {
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

        return 0;
    } else {
        MemCopy(src, dest, size);
        return 0;
    }
}