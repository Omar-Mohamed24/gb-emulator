#pragma once
#include "gb/types.hpp"
#include "gb/timer.hpp"
#include "gb/ppu.hpp"
#include "gb/joypad.hpp"
#include "gb/apu.hpp"
#include "gb/mbc/mbc.hpp"
#include <memory>
#include <vector>
#include <string>

namespace gb
{
    class Bus
    {
        private:
            PPU &ppu;
            Timer &timer;
        public:
            std::vector<u8> rom;                    // 0x0000-0x7FFF
            std::vector<u8> vram;                   // 0x8000-0x9FFF
            std::unique_ptr<MBC> mbc;               // cartridge bank controller
            std::vector<u8> wram;                   // 0xC000-0xDFFF (0xE000-0xFDFF echoes this range)
            std::vector<u8> oam;                    // 0xFE00-0xFE9F
            std::vector<u8> io_regs;                // 0xFF00-0xFF7F
            std::vector<u8> hram;                   // 0xFF80-0xFFFE
            u8 ie = 0;                              // 0xFFFF
            u8 iflag = 0;                           // 0xFF0F

            // SB (0xFF01) holds one byte to send over the link cable; SC (0xFF02)'s bit 7 starts
            // a transfer and is cleared by hardware once it finishes. With no real cable attached,
            // writes to SC trigger a fake instant transfer: SB's byte lands here and bit 7 is
            // cleared right away -- this is how test ROMs like cpu_instrs print results headlessly.
            std::vector<u8> serial_output;

            Joypad joypad; // 0xFF00
            APU apu;       // 0xFF10-0xFF3F

            Bus(PPU &ppu, Timer &timer);

            bool load_rom(const std::string &filename);

            u8 read(u16 addr) const;

            void write(u16 addr, u8 value);

            // Advances the cartridge clock and the sound unit by CPU cycles.
            void tick(int cycles);

            // Loads battery-backed RAM (and clock state) from a .sav file. Returns false if the file can't be read.
            bool load_save(const std::string &path);

            // Writes battery-backed RAM (and clock state) to a .sav file. Returns true when there is nothing to save.
            bool save_ram(const std::string &path) const;
    };
}