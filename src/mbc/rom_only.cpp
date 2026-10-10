#include "gb/mbc/rom_only.hpp"

namespace gb
{
    u8 RomOnly::read(u16 address) const
    {
        if (address <= 0x7FFF)
        {
            return address < rom.size() ? rom[address] : 0xFF;
        }
        if (address >= 0xA000 && address <= 0xBFFF)
        {
            return read_ram(0, address - 0xA000);
        }
        return 0xFF;
    }

    void RomOnly::write(u16 address, u8 value)
    {
        if (address >= 0xA000 && address <= 0xBFFF)
        {
            write_ram(0, address - 0xA000, value);
        }
    }
}
