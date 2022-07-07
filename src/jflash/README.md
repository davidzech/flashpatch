# JFlash

Journaled Flash Storage for emulating GBA EEPROM devices.

## GBA EEPROM vs Flash

Standard GBA Flash chips are 512 Kilobits / 64 Kilobytes. (Some are twice the size, being 1M/512Kb).
Flash Sector sizes are generally 4 Kilobytes.

Most EEPROM chips are 64 Kilobits / 8 Kilobytes. (Some are 1/16th the size)

This means in the tightest scenario, we can assume a Flash chip with at least 8 times as much storage, where 2 Flash Sectors equaling the size of the largest EEPROM.

GBA EEPROM is read and written to in chunks of 64 bits (8 bytes). The actual protocol uses smaller chunks, but on an application level, and application will always call Nintendo supplied libraries to write to EEPROM, which always use a chunk size of 64 bits.

GBA Flash is read and written to 8 bits at a tiem (1 byte), with the important caveat that the Flash address must be erased (has value 0xFF) before written to. GBA Flash chips can also only erase an entire 4k sector as the smallest erase unit.

Naively emulating EEPROM by reading, modifying, and flushing entire sectors is slow taxing on the write cycle limits of the Flash chip. A better strategy is to journal writes to the underlying flash storage to minimize the numbers of erasures necessary.

## Journaling strategy

The journaling strategy used here is derivative of [STM EEPROM emulation strategy](https://www.st.com/resource/en/application_note/an4894-eeprom-emulation-techniques-and-software-for-stm32-microcontrollers-stmicroelectronics.pdf)

On a 512K/64Kbit Flash chip, we will have 16 4Kbit sectors. We will partition these into two logical units, called `partitions` consisting of 8 sectors each.

At the top of each partition is a 4 byte header.

```c++
typedef struct {
    union {
        u8 b0;
        u8 b1;
        u8 b2;
        u8 b3
        u32 state;
    };
} Header;

constexpr u32 ERASED = 0xFFFF_FFFF;
constexpr u32 RECEIVE = 0x00FF_FFFF;
constexpr u32 ACTIVE = 0x0000_FFFF;
constexpr u32 VALID = 0x0000_00FF;
cosntexpr u32 ERASING = 0x0000_0000;
```



First step is to `init()` the Flash. This process will initialize the sector headers if uninitialized, or perform a complete Flash wipe if left in a corrupted state.

### Writing Data

On an `EEPROMWrite(u16 addr, u8 data[8])` we will fill the currently active sector with the following until full:

```c++
typedef struct {
    u16 addr;
    u8 data[8];
} Frame
```

Once a partition is full, we will mark the current partition as `VALID` and the next partition as `RECEIVE`. We will then rollup the latest values of the variables we have stored, and copy them to the `RECEIVE`ing partition. The number of copies variables will always be less than or equal to what was before. Then we will mark the `RECEIVE` page as `ACTIVE`, and mark the previoius page as `ERASING`, and issue a sector erase command. Upon erase, the header will contain the default value for `ERASED(-1)`

### Reading Data

Starting from the highest address of the active partition, scan upwards until we find the variable we are looking for. If not found, we return all `0xFF`.
