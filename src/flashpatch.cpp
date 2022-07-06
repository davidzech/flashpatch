#include <flash/flash.h>
#include <sram/sram.h>
using FlashChip = Flash::Chip<Flash::SST39SF512>;

void ROMInit() { FlashChip::Init(); }