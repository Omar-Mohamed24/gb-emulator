#include "gb/mbc/mbc5.hpp"

namespace gb
{
    u8 MBC5::read(u16 address) const
    {
        if (address < 0x4000)
        {
            return read_rom(0, address);
        }
        if (address < 0x8000)
        {
            return read_rom(rom_bank, address - 0x4000);
        }
        if (address >= 0xA000 && address <= 0xBFFF)
        {
            if (!ram_enabled)
            {
                return 0xFF;
            }
            return read_ram(ram_bank, address - 0xA000);
        }
        return 0xFF;
    }

    void MBC5::write(u16 address, u8 value)
    {
        if (address < 0x2000)
        {
            ram_enabled = (value & 0x0F) == 0x0A;
        }
        else if (address < 0x3000)
        {
            rom_bank = static_cast<u16>((rom_bank & 0x100) | value);
        }
        else if (address < 0x4000)
        {
            rom_bank = static_cast<u16>((rom_bank & 0x0FF) | ((value & 0x01) << 8));
        }
        else if (address < 0x6000)
        {
            ram_bank = value & (rumble ? 0x07 : 0x0F);
        }
        else if (address >= 0xA000 && address <= 0xBFFF)
        {
            if (ram_enabled)
            {
                write_ram(ram_bank, address - 0xA000, value);
            }
        }
    }
}
