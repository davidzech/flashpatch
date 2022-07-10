#include <flash/flash.h>
#include <jflash/jflash.h>
#include <sram/sram.h>

using FlashChip = Flash::Chip<Flash::SST39SF512>;
using Journal = JFlash::Journal<FlashChip::Info>;

extern "C" {

void ROMInit();

__attribute__((naked, target("no-thumb-mode"))) void Entrypoint() {
    asm("ldr lr, .orig");
    asm("b ROMInit");
    asm(".orig: .long 0x080000c0");
}

void ROMInit() {
    FlashChip::Init();
    Journal::Init();
}

u16 EEPROMWrite(u16 addr, u8 data[8], bool8 wait) {
    JFlash::Variable *v = reinterpret_cast<JFlash::Variable *>(data);
    return Journal::WriteVar(addr, *v);
}

u16 EEPROMRead(u16 address, u8 data[8]) {
    auto var = Journal::ReadVar(address);
    for (int i = 0; i < sizeof(var.data); i++) {
        data[i] = var.data[i];
    }

    return 0;
}
}