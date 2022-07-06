#pragma once

#include <flash/flash.h>
#include <gba/types.h>

namespace jflash {

enum state : u32 {
    ERASED = 0xFFFFFFFF,
    RECEIVING = 0x00FFFFFF,
    ACTIVE = 0x0000FFFF,
    SENDING = 0x000000FF,
    ERASING = 0x00000000,
};

struct header {
    union {
        u8 b0;
        u8 b1;
        u8 b2;
        u8 b3;
        state state;
    };
};
static_assert(sizeof(header) == 4);

struct frame {
    u16 addr;
    u8 data[8];
};
static_assert(sizeof(frame) == 10);

template <const FlashInfo &T> class jflash {
  public:
    constexpr static int partition_max_frames = (((T.type.sector.count / 2) * T.type.sector.size) - sizeof(header)) / sizeof(frame);
    constexpr static int total_frames = partition_max_frames * 2;
    constexpr static partition *partition_0 = reinterpret_cast<partition *>(FLASH_BASE);
    constexpr static partition *partition_1 = reinterpret_cast<partition *>(FLASH_BASE + ((T.type.sector.count / 2) << T.type.sector.shift));

    struct partition {
        header header;
        frame frames[partition_max_frames];
        u8 padding[T.type.sector.size - (header) + (partition_max_frames * sizeof(frame))];
    };

    static void init() {
        if (!(partition_0->header.state == ERASED && partition_1->header.state == ACTIVE)) {
            format();
        } else if (!(partition_0->header.state == ACTIVE && partition_1->header.state == ERASED)) {
            format();
        }
    }

    static void format() { EraseFlashChip(); };

    static u64 read_var(u16 addr);
    static void read_var(u16 addr, u8 buf[8]);

    static void write_var(u16 addr, u8 data[8]);
    static void write_var(u16 addr, u64 data);

    static void transfer_partition();
};

} // namespace jflash