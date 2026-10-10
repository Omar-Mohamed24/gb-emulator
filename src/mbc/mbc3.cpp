#include "gb/mbc/mbc3.hpp"
#include <algorithm>

namespace gb
{
    namespace
    {
        constexpr u32 CLOCK_HZ = 4194304;
        constexpr u64 CLOCK_WRAP_SECONDS = 512ull * 86400; // the day counter wraps after 512 days

        // Bits that the clock registers can hold: S, M, H, DL, DH.
        constexpr u8 RTC_MASK[5] = {0x3F, 0x3F, 0x1F, 0xFF, 0xC1};

        // Save footer after the RAM: 5 clock bytes, then the Unix time of the save (8 bytes, little-endian).
        constexpr std::size_t RTC_FOOTER_SIZE = 5 + 8;
    }

    u8 MBC3::read(u16 address) const
    {
        if (address < 0x4000)
        {
            return read_rom(0, address);
        }
        if (address < 0x8000)
        {
            std::size_t bank = rom_bank ? rom_bank : 1;
            return read_rom(bank, address - 0x4000);
        }
        if (address >= 0xA000 && address <= 0xBFFF)
        {
            if (!ram_enabled)
            {
                return 0xFF;
            }
            if (ram_bank <= 3)
            {
                return read_ram(ram_bank, address - 0xA000);
            }
            if (has_rtc && ram_bank >= 8 && ram_bank <= 12)
            {
                return latched[ram_bank - 8];
            }
            return 0xFF;
        }
        return 0xFF;
    }

    void MBC3::write(u16 address, u8 value)
    {
        if (address < 0x2000)
        {
            ram_enabled = (value & 0x0F) == 0x0A;
        }
        else if (address < 0x4000)
        {
            rom_bank = value & 0x7F;
        }
        else if (address < 0x6000)
        {
            ram_bank = value & 0x0F;
        }
        else if (address < 0x8000)
        {
            if (value == 0x00)
            {
                latch_armed = true;
            }
            else if (value == 0x01 && latch_armed)
            {
                std::copy(rtc, rtc + 5, latched);
                latch_armed = false;
            }
            else
            {
                latch_armed = false;
            }
        }
        else if (address >= 0xA000 && address <= 0xBFFF)
        {
            if (!ram_enabled)
            {
                return;
            }
            if (ram_bank <= 3)
            {
                write_ram(ram_bank, address - 0xA000, value);
            }
            else if (has_rtc && ram_bank >= 8 && ram_bank <= 12)
            {
                rtc[ram_bank - 8] = value & RTC_MASK[ram_bank - 8];
            }
        }
    }

    void MBC3::tick(int cycles)
    {
        if (!has_rtc || (rtc[4] & 0x40))
        {
            return;
        }
        rtc_cycles += static_cast<u32>(cycles);
        while (rtc_cycles >= CLOCK_HZ)
        {
            rtc_cycles -= CLOCK_HZ;
            advance_seconds(1);
        }
    }

    void MBC3::advance_seconds(u64 seconds)
    {
        u64 days = rtc[3] | ((rtc[4] & 0x01) << 8);
        u64 total = rtc[0] + 60ull * rtc[1] + 3600ull * rtc[2] + 86400ull * days + seconds;

        u8 carry = rtc[4] & 0x80;
        if (total >= CLOCK_WRAP_SECONDS)
        {
            total %= CLOCK_WRAP_SECONDS;
            carry = 0x80;
        }

        days = total / 86400;
        rtc[0] = static_cast<u8>(total % 60);
        rtc[1] = static_cast<u8>((total / 60) % 60);
        rtc[2] = static_cast<u8>((total / 3600) % 24);
        rtc[3] = static_cast<u8>(days & 0xFF);
        rtc[4] = static_cast<u8>((rtc[4] & 0x40) | carry | ((days >> 8) & 0x01));
    }

    std::vector<u8> MBC3::save_data(u64 now) const
    {
        std::vector<u8> out = ram;
        if (has_rtc)
        {
            out.insert(out.end(), rtc, rtc + 5);
            for (int i = 0; i < 8; i++)
            {
                out.push_back(static_cast<u8>(now >> (8 * i)));
            }
        }
        return out;
    }

    void MBC3::load_data(const std::vector<u8> &data, u64 now)
    {
        MBC::load_data(data, now);
        if (!has_rtc || data.size() < ram.size() + RTC_FOOTER_SIZE)
        {
            return;
        }

        const std::size_t base = ram.size();
        std::copy(data.begin() + base, data.begin() + base + 5, rtc);

        u64 saved_at = 0;
        for (int i = 0; i < 8; i++)
        {
            saved_at |= static_cast<u64>(data[base + 5 + i]) << (8 * i);
        }

        if (now > saved_at && !(rtc[4] & 0x40))
        {
            advance_seconds(now - saved_at);
        }
    }
}
