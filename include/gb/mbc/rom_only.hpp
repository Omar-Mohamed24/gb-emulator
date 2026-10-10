#pragma once
#include "gb/mbc/mbc.hpp"

namespace gb
{
    // Cartridges with no bank switching: 32 KB of ROM, optionally with fixed external RAM.
    class RomOnly : public MBC
    {
    public:
        using MBC::MBC;

        u8 read(u16 address) const override;
        void write(u16 address, u8 value) override;
    };
}
