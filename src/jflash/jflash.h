#pragma once

#include <flash/flash.h>
#include <gba/types.h>

namespace JFlash {

enum State : u32 {
    ERASED = 0xFFFFFFFF,
    RECEIVING = 0x00FFFFFF,
    ACTIVE = 0x0000FFFF,
    SENDING = 0x000000FF,
    ERASING = 0x00000000,
};

struct Header {
    union {
        u8 b0;
        u8 b1;
        u8 b2;
        u8 b3;
        State state;
    };
};
static_assert(sizeof(Header) == 4);

struct Variable {
    u8 data[8];
};

struct Frame {
    u16 addr;
    Variable data;
};

static_assert(sizeof(Frame) == 10);

template <const Flash::Info &F> class Journal {
  private:
  public:
    Journal() = delete;
    using Chip = Flash::Chip<F>;

    constexpr static int PartitionMaxFrames = ((F.type.romSize / 2) - sizeof(Header)) / sizeof(Frame);
    constexpr static int TotalFrames = PartitionMaxFrames * 2;

    struct Partition {
        Header header;
        Frame frames[PartitionMaxFrames];
        u8 padding[(F.type.romSize / 2) - sizeof(header) - sizeof(frames)];
    };
    static_assert(sizeof(Partition) == (F.type.romSize / 2));
    static_assert(sizeof(Partition::frames) == sizeof(Frame) * PartitionMaxFrames);
    static_assert(sizeof(Partition::padding) < sizeof(Frame));

    static Partition *Partition0() { return reinterpret_cast<Partition *>(FLASH_BASE); }
    static Partition *Partition1() { return reinterpret_cast<Partition *>(FLASH_BASE + (F.type.romSize / 2)); }

    static void Init() {
        auto p0hdr = Chip::Read(&Partition0()->header);
        auto p1hdr = Chip::Read(&Partition1()->header);
        if (!(p0hdr.state == ERASED && p1hdr.state == ACTIVE)) {
            Format();
        } else if (!(p0hdr.state == ACTIVE && p1hdr.state == ERASED)) {
            Format();
        }
    }

    static void Format() {
        Chip::EraseChip();
        constexpr Header x = {.state = ACTIVE};
        Chip::Write(x.b2, &Partition0()->header.b2);
        Chip::Write(x.b3, &Partition0()->header.b3);
    };

    static Variable ReadVar(u16 addr) {
        // Start from highest address in partition, and seek for what we are looking for
        auto activePartition = ActivePartition();
        typename Chip::ReadByteFunc func;
        for (int i = PartitionMaxFrames; i > 0; i--) {
            Frame *f = &activePartition->frames[i];
            u16 varAddr = Chip::Read(&f->addr, func);
            if (addr == varAddr) {
                return Chip::Read(&f->data);
            }
        }

        return Variable{.data = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}};
    }

    static void WriteVar(u16 addr, Variable data) {
        // Start from lowest address and write
        auto activePartition = ActivePartition();
        typename Chip::ReadByteFunc func;
        for (int i = 0; i < PartitionMaxFrames; i++) {
            Frame *f = &activePartition->frames[i];
            u16 varAddr = Chip::Read(&f->addr, func);
            if (varAddr == 0xFFFF) {
                Chip::Write(data, &f->data);
                return;
            }
        }

        // we are filled
    }

    static void TransferPartition();

    static Partition *ActivePartition() {
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
};

} // namespace JFlash