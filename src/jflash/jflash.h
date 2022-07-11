#pragma once

#include <common/utils.h>
#include <flash/flash.h>
#include <gba/types.h>
namespace JFlash {

enum State : u32 {
    ERASED = 0xFFFFFFFF,
    RECEIVING = ERASED << 8,
    ACTIVE = RECEIVING << 8,
    SENDING = ACTIVE << 8,
    ERASING = SENDING << 8,
};

static_assert(RECEIVING == 0xFFFFFF00);
static_assert(ERASING == 0);

struct Header {
    union {
        struct {
            u8 receiving;
            u8 active;
            u8 sending;
            u8 erasing;
        };
        State state;
    };
    u32 reserved[3];
};
static_assert(sizeof(Header) == 16);

struct Variable {
    u8 data[8];

    bool operator==(const Variable &other) const {
        for (int i = 0; i < sizeof(data); i++) {
            if (data[i] != other.data[i]) {
                return false;
            }
        }
        return true;
    }
};

struct Frame {
    u16 addr;
    u8 reserved[6] = {0, 0, 0, 0, 0, 0};
    Variable data;
};

static_assert(sizeof(Frame) == 16);

template <const Flash::Info &F, const int EEPROMSize = (8 * 1024)> class Journal {
  public:
    Journal() = delete;
    using Chip = Flash::Chip<F>;

    constexpr static int PartitionMaxFrames = ((F.type.romSize / 2) / sizeof(Frame)) - 1;
    constexpr static int TotalFrames = PartitionMaxFrames * 2;
    constexpr static int NumVars = EEPROMSize / sizeof(Variable);

    struct Partition {
        Header header;
        Frame frames[PartitionMaxFrames];

        static constexpr int numSectors = F.type.sector.count / 2;
    };
    static_assert(sizeof(Partition) == (F.type.romSize / 2));
    static_assert(sizeof(Partition::frames) == sizeof(Frame) * PartitionMaxFrames);
    static_assert(sizeof(Partition::frames) + sizeof(Partition::header) == F.type.romSize / 2);
    static_assert(Partition::numSectors > 0);

    static Partition *Partition0() { return reinterpret_cast<Partition *>(FLASH_BASE); }
    static Partition *Partition1() { return reinterpret_cast<Partition *>(FLASH_BASE + (F.type.romSize / 2)); }
    constexpr static s16 *LastFrameIndex() { return (s16 *)0x203fff0; }

    static void Init() {
        auto activePart = MaybeActivePartition();
        if (activePart == nullptr) {
            Format();
        }
    }

    static void Format() {
        Chip::EraseChip();
        Chip::Write((u8)0x00, (u8 *)&Partition0()->header.receiving);
        Chip::Write((u8)0x00, (u8 *)&Partition0()->header.active);
    };

    static Maybe<Variable> MaybeReadVar(u16 addr, typename Chip::ReadByteFunc &func, Partition *activePartition = nullptr, bool useLast = true) {
        if (activePartition == nullptr) {
            activePartition = ActivePartition();
        }

        s16 *lastAddr = LastFrameIndex();

        int end = (useLast && *lastAddr != 0) ? 1 + *lastAddr : PartitionMaxFrames;

        for (int i = end; i > 0; i--) {
            Frame *f = &activePartition->frames[i];
            u16 varAddr = Chip::Read(&f->addr, func);
            if (addr == varAddr) {
                return Chip::Read(&f->data);
            }
            if (addr == 0xFFFF) {
                break;
            }
        }

        return nullptr;
    }

    static Variable ReadVar(u16 addr, typename Chip::ReadByteFunc &func, Partition *activePartition = nullptr) {
        if (activePartition == nullptr) {
            activePartition = ActivePartition();
        }

        Maybe<Variable> var = MaybeReadVar(addr, func, activePartition);
        if (var) {
            return *var;
        }
        return Variable{.data = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}};
    }

    static Variable ReadVar(u16 addr, Partition *activePartition = nullptr) {
        typename Chip::ReadByteFunc func;
        return ReadVar(addr, func, activePartition);
    }

    static u16 WriteVar(u16 addr, const Variable &data, Partition *activePartition = nullptr, bool useLast = true) { // Start from lowest address and write
        if (activePartition == nullptr) {
            activePartition = ActivePartition();
        }

        s16 *lastAddr = LastFrameIndex();

        const Frame newFrame{.addr = addr, .data = data};
        typename Chip::ReadByteFunc func;

        if (!useLast || *lastAddr == 0) {
            for (int i = 0; i < PartitionMaxFrames; i++) {
                Frame *f = &activePartition->frames[i];
                u16 varAddr = Chip::Read(&f->addr, func);
                if (varAddr == 0xFFFF) {
                    Chip::Write(newFrame, f);
                    *lastAddr = i;
                    return 0;
                }
            }
        } else if (useLast && *lastAddr < PartitionMaxFrames - 1) {
            Frame *f = &activePartition->frames[*lastAddr + 1];
            Chip::Write(newFrame, f);
            *lastAddr += 1;
            return 0;
        }

        return TransferPartition(activePartition, newFrame);
    }

    static u16 TransferPartition(Partition *sending, const Frame &pendingFrame) {
        int sectorStart = 0;
        Partition *receiving;
        if (sending == Partition0()) {
            receiving = Partition1();
        } else {
            sectorStart = F.type.sector.count / 2;
            receiving = Partition0();
        }

        // mark sending partition as sending
        Chip::Write((u8)0x00, &sending->header.sending);
        // mark receiving partition as receiving
        Chip::Write((u8)0x00, &receiving->header.receiving);

        *LastFrameIndex() = 0;
        // transfer latest vars
        typename Chip::ReadByteFunc func;
        for (int i = 0; i < NumVars; i++) {
            if (i == pendingFrame.addr) {
                continue;
            }
            // disable lastFrame usage for reading
            auto maybeVar = MaybeReadVar(i, func, sending, false);
            if (maybeVar) {
                auto result = WriteVar(i, *maybeVar, receiving, false);
                if (result != 0) {
                    return result;
                }
            }
        }

        // write pending variable
        auto result = WriteVar(pendingFrame.addr, pendingFrame.data, receiving, false);
        if (result != 0) {
            return result;
        }

        // mark receiving partition as active
        result = Chip::Write((u8)0x00, (u8 *)&receiving->header.active);
        if (result != 0) {
            return result;
        }
        // mark sending partition as erasing
        result = Chip::Write((u8)0x00, (u8 *)&sending->header.erasing);
        if (result != 0) {
            return result;
        }

        // dispatch erase command on all sectors of partition but don't wait on sending.
        for (int sector = 0; sector < Partition::numSectors; sector++) {
            u16 result = Chip::EraseSector(sectorStart + sector, true);
            if (result != 0) {
                return result;
            }
        }

        *LastFrameIndex() = 0;

        return result;
    }

    static Partition *MaybeActivePartition() {
        auto p0 = Partition0();
        auto p1 = Partition1();
        auto p0hdr = Chip::Read(&p0->header);

        if (p0hdr.state == ACTIVE || p0hdr.state == SENDING) {
            return p0;
        }

        auto p1hdr = Chip::Read(&p1->header);
        if (p1hdr.state == ACTIVE || p1hdr.state == SENDING) {
            return p1;
        }

        return nullptr;
    }

    static Partition *ActivePartition() {
        auto part = MaybeActivePartition();
        if (part == nullptr) {
            Format();
            return Partition0();
        }

        return part;
    }
};

} // namespace JFlash