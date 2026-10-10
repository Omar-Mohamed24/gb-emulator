#include "gb/mbc/mbc1.hpp"

namespace gb
{
    u8 MBC1::read(u16 address) const
    {
        if (address < 0x4000)
        {
            std::size_t bank = advanced ? (static_cast<std::size_t>(bank_high) << 5) : 0;
            return read_rom(bank, address);
        }
        if (address < 0x8000)
        {
            std::size_t low = bank_low ? bank_low : 1;
            std::size_t bank = (static_cast<std::size_t>(bank_high) << 5) | low;
            return read_rom(bank, address - 0x4000);
        }
        if (address >= 0xA000 && address <= 0xBFFF)
        {
            if (!ram_enabled)
            {
                return 0xFF;
            }
            std::size_t bank = advanced ? bank_high : 0;
            return read_ram(bank, address - 0xA000);
        }
        return 0xFF;
    }

    void MBC1::write(u16 address, u8 value)
    {
        if (address < 0x2000)
        {
            ram_enabled = (value & 0x0F) == 0x0A;
        }
        else if (address < 0x4000)
        {
            bank_low = value & 0x1F;
        }
        else if (address < 0x6000)
        {
            bank_high = value & 0x03;
        }
        else if (address < 0x8000)
        {
            advanced = (value & 0x01) != 0;
        }
        else if (address >= 0xA000 && address <= 0xBFFF)
        {
            if (ram_enabled)
            {
                write_ram(advanced ? bank_high : 0, address - 0xA000, value);
            }
        }
    }
}
