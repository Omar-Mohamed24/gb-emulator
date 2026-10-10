#pragma once
#include "gb/mbc/mbc.hpp"

namespace gb
{
    // MBC3: up to 2 MB of ROM, 32 KB of RAM, and an optional real-time clock.
    class MBC3 : public MBC
    {
    public:
        using MBC::MBC;

        bool has_rtc = false; // set for cartridge types 0x0F and 0x10

        u8 read(u16 address) const override;
        void write(u16 address, u8 value) override;
        void tick(int cycles) override;
        std::vector<u8> save_data(u64 now) const override;
        void load_data(const std::vector<u8> &data, u64 now) override;

    private:
        bool ram_enabled = false;
        u8 rom_bank = 1; // 7 bits; a value of 0 behaves as 1
        u8 ram_bank = 0; // 0-3 selects RAM; 8-12 selects a clock register
        u8 rtc[5] = {0, 0, 0, 0, 0};
        u8 latched[5] = {0, 0, 0, 0, 0};
        bool latch_armed = false;
        u32 rtc_cycles = 0;
        void advance_seconds(u64 seconds);
    };
}
