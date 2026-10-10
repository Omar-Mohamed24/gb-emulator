#pragma once
#include "gb/mbc/mbc.hpp"

namespace gb
{
    // MBC5: up to 8 MB of ROM and 128 KB of RAM.
    class MBC5 : public MBC
    {
    public:
        using MBC::MBC;

        bool rumble = false;

        u8 read(u16 address) const override;
        void write(u16 address, u8 value) override;

    private:
        bool ram_enabled = false;
        u16 rom_bank = 1; // 9 bits; a value of 0 behaves as 1
        u8 ram_bank = 0;  // 4 bits, or 3 bits on rumble cartridges
    };
}
