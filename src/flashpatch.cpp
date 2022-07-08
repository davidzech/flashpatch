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

u16 EEPROMWrite(u16 addr, u8 data[8], bool8 wait) {
    JFlash::Variable &v = reinterpret_cast<JFlash::Variable &>(data);
    return Journal::WriteVar(addr, v);
}