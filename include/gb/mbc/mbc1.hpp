#pragma once
#include "gb/mbc/mbc.hpp"

namespace gb
{
    // MBC1: up to 2 MB of ROM and 32 KB of RAM.
    class MBC1 : public MBC
    {
    public:
        using MBC::MBC;

        u8 read(u16 address) const override;
        void write(u16 address, u8 value) override;

    private:
        bool ram_enabled = false;
        u8 bank_low = 1;      // lower 5 bits of the ROM bank; a value of 0 behaves as 1
        u8 bank_high = 0;     // upper 2 bits: extra ROM bank bits or the RAM bank
        bool advanced = false; // mode 1: upper bits apply to the fixed bank and RAM banking is active
    };
}
