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

void ROMInit() { FlashChip::Init(); }

u16 EEPROMConfigure(u16) {
    Journal::Init();
    return 0;
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

void __aeabi_memcpy(void *dest, void *src, size_t n) {
    u8 *d = (u8 *)dest;
    u8 *s = (u8 *)src;
    while (n != 0) {
        *d++ = *s++;
        n--;
    }
}
}