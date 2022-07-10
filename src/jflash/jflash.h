#pragma once

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

template <const Flash::Info &F> class Journal {
  private:
    constexpr static s16 *LastAddr() { return (s16 *)(0x203fff0); }

  public:
    Journal() = delete;
    using Chip = Flash::Chip<F>;

    constexpr static int PartitionMaxFrames = ((F.type.romSize / 2) - sizeof(Header)) / sizeof(Frame);
    constexpr static int TotalFrames = PartitionMaxFrames * 2;

    struct Partition {
        Header header;
        Frame frames[PartitionMaxFrames];
        u8 padding[(F.type.romSize / 2) - sizeof(header) - sizeof(frames)];

        static constexpr int numSectors = F.type.sector.count / 2;
    };
    static_assert(sizeof(Partition) == (F.type.romSize / 2));
    static_assert(sizeof(Partition::frames) == sizeof(Frame) * PartitionMaxFrames);
    static_assert(sizeof(Partition::padding) < sizeof(Frame));

    static Partition *Partition0() { return reinterpret_cast<Partition *>(FLASH_BASE); }
    static Partition *Partition1() { return reinterpret_cast<Partition *>(FLASH_BASE + (F.type.romSize / 2)); }

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

    static Variable ReadVar(u16 addr, typename Chip::ReadByteFunc &func) {
        s16 *lastAddr = LastAddr();

        int end = (*lastAddr != 0) ? 1 + *lastAddr : PartitionMaxFrames;

        auto activePartition = ActivePartition();
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

        return Variable{.data = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}};
    }

    static Variable ReadVar(u16 addr) {
        typename Chip::ReadByteFunc func;
        return ReadVar(addr, func);
    }

    static u16 WriteVar(u16 addr, const Variable &data) { // Start from lowest address and write
        s16 *lastAddr = LastAddr();

        const Frame newFrame{.addr = addr, .data = data};
        auto activePartition = ActivePartition();
        typename Chip::ReadByteFunc func;

        if (*lastAddr == 0) {
            for (int i = 0; i < PartitionMaxFrames; i++) {
                Frame *f = &activePartition->frames[i];
                u16 varAddr = Chip::Read(&f->addr, func);
                if (varAddr == 0xFFFF) {
                    Chip::Write(newFrame, f);
                    *lastAddr = i;
                    return 0;
                }
            }
        } else if (*lastAddr < PartitionMaxFrames) {
            Frame *f = &activePartition->frames[*lastAddr + 1];
            Chip::Write(newFrame, f);
            *lastAddr += 1;
            return 0;
        }

        return TransferPartition(activePartition, newFrame);
    }

    static u16 TransferPartition(Partition *sending, const Frame &pendingFrame) {
        Partition *receiving;
        if (sending == Partition0()) {
            receiving = Partition1();
        } else {
            receiving = Partition1();
        }

        // mark sending partition as sending
        Chip::Write((u8)0x00, &sending->header.sending);
        // mark receiving partition as receiving
        Chip::Write((u8)0x00, &receiving->header.receiving);

        // compact and transfer vars from sending O(N^2)
        typename Chip::ReadByteFunc func;
        for (int i = 0; i < PartitionMaxFrames; i++) {
            Frame *f = &ActivePartition()->frames[i];
            u16 varAddr = Chip::Read(&f->addr, func);
            if (varAddr != pendingFrame.addr && varAddr != 0xFFFF) {
                auto result = WriteVar(varAddr, ReadVar(varAddr, func));
                if (result != 0) {
                    return result;
                }
            }
        }

        // write pending variable
        auto result = WriteVar(pendingFrame.addr, pendingFrame.data);
        if (!result) {
            return result;
        }

        // mark receiving partition as active
        Chip::Write((u8)0x00, &receiving->header.active);
        // mark sending partition as erasing
        Chip::Write((u8)0x00, &sending->header.erasing);
        // dispatch erase command on all sectors of partition but don't wait on sending.
        for (u16 sector = 0; sector < sending->numSectors; sector++) {
            Chip::EraseSector(sector, false); // might be dangerous to not wait
        }

        return 0;
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
        }

        return Partition0();
    }
};

} // namespace JFlash