#pragma once
#include <algorithm>
#include <cstddef>
#include <vector>
#include "gb/types.hpp"

namespace gb
{
    // Base class for cartridge memory bank controllers. The bus sends reads and
    // writes for the ROM range (0x0000-0x7FFF) and external RAM (0xA000-0xBFFF) here.
    class MBC
    {
    public:
        MBC(const std::vector<u8> &rom, std::size_t ram_size) : rom(rom), ram(ram_size, 0) {}
        virtual ~MBC() = default;

        virtual u8 read(u16 address) const = 0;
        virtual void write(u16 address, u8 value) = 0;

        bool battery = false;
        virtual void tick(int cycles) { (void)cycles; }

        virtual std::vector<u8> save_data(u64 now) const
        {
            (void)now;
            return ram;
        }

        virtual void load_data(const std::vector<u8> &data, u64 now)
        {
            (void)now;
            std::size_t count = std::min(ram.size(), data.size());
            std::copy(data.begin(), data.begin() + count, ram.begin());
        }

    protected:
        const std::vector<u8> &rom;
        std::vector<u8> ram;

        // One byte from a 16 KB ROM bank. Bank numbers wrap around the ROM's size.
        u8 read_rom(std::size_t bank, std::size_t offset) const
        {
            std::size_t bank_count = rom.size() / 0x4000;
            if (bank_count == 0)
            {
                return 0xFF;
            }
            std::size_t index = (bank % bank_count) * 0x4000 + offset;
            return index < rom.size() ? rom[index] : 0xFF;
        }

        // One byte from an 8 KB external RAM bank.
        u8 read_ram(std::size_t bank, std::size_t offset) const
        {
            if (ram.empty())
            {
                return 0xFF;
            }
            return ram[(bank * 0x2000 + offset) % ram.size()];
        }

        void write_ram(std::size_t bank, std::size_t offset, u8 value)
        {
            if (ram.empty())
            {
                return;
            }
            ram[(bank * 0x2000 + offset) % ram.size()] = value;
        }
    };
}
