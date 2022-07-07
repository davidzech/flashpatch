#include <flash/flash.h>
#include <jflash/jflash.h>
#include <sram/sram.h>

using FlashChip = Flash::Chip<Flash::SST39SF512>;
using Journal = JFlash::Journal<FlashChip::Info>;

void ROMInit() {
    FlashChip::Init();
    Journal::Init();
    auto var = Journal::ReadVar(0x0);
}