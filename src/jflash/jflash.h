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

  private:
    class compactProgress {
      public:
        static constexpr int Size = EEPROMSize / 8;

      private:
        u8 progress[Size];
        class reference {
          public:
            u8 *parent;

            reference(u8 *p) : parent(p) {}
            reference &operator=(const bool b) {
                if (b) {
                    *parent |= 0x80;
                } else {
                    *parent &= 0x7F;
                }

                return *this;
            }

            operator bool() const { return (*parent & 0x80) == 0x80; }
        };

      public:
        void Reset() {
            for (int i = 0; i < Size; i++) {
                progress[i] &= 0x7F;
            }
        }

        constexpr bool operator[](int index) const { return (progress[index] & 0x80) == 0x80; }
        reference operator[](int index) { return reference(&progress[index]); }
    };

    class lookupTable {
      public:
        static constexpr int Size = EEPROMSize / 8;

      private:
        u8 table[Size];

        class reference {
          public:
            u8 *parent;

            reference(u8 *p) : parent(p) {}
            reference &operator=(u8 val) {
                *parent = (*parent & 0x80) | (val & 0x7F);
                return *this;
            }

            operator u8() const { return (*parent & 0x7F); }
        };

      public:
        static constexpr int size = EEPROMSize / 8;

        void Reset() {
            for (int i = 0; i < Size; i++) {
                table[i] &= 0x80;
            }
        }

        constexpr u8 operator[](int index) const { return (table[index] & 0x7F); }
        reference operator[](int index) { return reference(&table[index]); }
    };

    struct globals {
        union {
            compactProgress CompactProgress;
            lookupTable LookupTable;
        };
        s16 LastFrameIndex;
    };

    __attribute__((always_inline)) static globals *Globals() { return (globals *)((EWRAM + EWRAM_SIZE - 1) - sizeof(globals)); }
    __attribute__((always_inline)) static Partition *Partition0() { return reinterpret_cast<Partition *>(FLASH_BASE); }
    __attribute__((always_inline)) static Partition *Partition1() { return reinterpret_cast<Partition *>(FLASH_BASE + (F.type.romSize / 2)); }

  public:
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

    static Maybe<Variable> MaybeReadVar(u16 addr, typename Chip::ReadByteFunc &func, Partition *activePartition = nullptr, bool useHint = false) {
        if (activePartition == nullptr) {
            activePartition = ActivePartition();
        }

        int end = PartitionMaxFrames - 1;
        auto &lookupTable = Globals()->LookupTable;

        if (useHint) {
            int hintAddr = lookupTable[addr];
            if (hintAddr != 0) {
                end = (hintAddr << 3);
            }
        }

        for (int i = end - 1; i >= 0; i--) {
            Frame *f = &activePartition->frames[i];
            u16 varAddr = Chip::Read(&f->addr, func);
            if (useHint && varAddr != 0xFFFF) {
                lookupTable[varAddr] = (u8)(i >> 3) + 1;
            }
            if (addr == varAddr) {
                return Chip::Read(&f->data);
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

    static u16 WriteVar(u16 addr, const Variable &data, Partition *activePartition = nullptr, bool useHint = false) { // Start from lowest address and write
        if (activePartition == nullptr) {
            activePartition = ActivePartition();
        }

        s16 &lastFrame = Globals()->LastFrameIndex;

        const Frame newFrame{.addr = addr, .data = data};
        typename Chip::ReadByteFunc func;

        if (!useHint || lastFrame == 0) {
            for (int i = 0; i < PartitionMaxFrames; i++) {
                Frame *f = &activePartition->frames[i];
                u16 varAddr = Chip::Read(&f->addr, func);
                if (varAddr == 0xFFFF) {
                    if (useHint) {
                        lastFrame = i;
                        Globals()->LookupTable[addr] = (u8)(i >> 3) + 1;
                    }
                    return Chip::Write(newFrame, f);
                }
            }
        } else if (useHint && lastFrame < PartitionMaxFrames - 1) {
            Frame *f = &activePartition->frames[lastFrame + 1];
            Globals()->LookupTable[addr] = (u8)((lastFrame + 1) >> 3) + 1;
            lastFrame += 1;
            return Chip::Write(newFrame, f);
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

        Globals()->LastFrameIndex = 0;
        auto &compactProgress = Globals()->CompactProgress;
        compactProgress.Reset();

        // transfer latest vars, by scanning once from the highest address
        typename Chip::ReadByteFunc func;
        for (int i = PartitionMaxFrames - 1; i <= 0; i--) {
            if (i == pendingFrame.addr) {
                continue;
            }

            Frame *f = &sending->frames[i];
            u16 varAddr = Chip::Read(&f->addr, func);
            if (varAddr != 0xFFFF) {
                // check if we have already hadded this variable
                if (bool(compactProgress[varAddr])) {
                    continue;
                }
                auto var = Chip::Read(&f->data);

                auto result = WriteVar(varAddr, var, receiving);
                if (result != 0) {
                    return result;
                }
            }
        }

        // write pending variable
        auto result = WriteVar(pendingFrame.addr, pendingFrame.data, receiving);
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
            u16 result = Chip::EraseSector(sectorStart + sector, false);
            if (result != 0) {
                return result;
            }
        }

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