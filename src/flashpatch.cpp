#include <flash/flash_mx.h>
#include <sram/sram.h>

constexpr const FlashInfo &FlashChip() { return MX29L010; }

extern "C" {
void WriteSRAMUnchecked(u8 *src, u8 *dest, u32 size) { return WriteSRAMUnchecked<FlashChip()>(src, dest, size); }
u8 *WriteSRAMChecked(u8 *src, u8 *dest, u32 size) { return WriteSRAMChecked<FlashChip()>(src, dest, size); }
}