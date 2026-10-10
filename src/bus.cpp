#include "gb/bus.hpp"
#include "gb/types.hpp"
#include "gb/ppu.hpp"
#include "gb/timer.hpp"
#include "gb/mbc/rom_only.hpp"
#include "gb/mbc/mbc1.hpp"
#include "gb/mbc/mbc3.hpp"
#include "gb/mbc/mbc5.hpp"
#include <cstdio>
#include <ctime>
#include <memory>

namespace gb
{
    namespace
    {
        std::size_t ram_size_from_header(u8 code)
        {
            switch (code)
            {
            case 0x01:
                return 2048;
            case 0x02:
                return 8192;
            case 0x03:
                return 32768;
            case 0x04:
                return 131072;
            case 0x05:
                return 65536;
            default:
                return 0;
            }
        }

        bool is_stored_io(u16 addr)
        {
            return addr == 0xFF01 || addr == 0xFF02 || addr == 0xFF46; // SB, SC, DMA
        }

        std::unique_ptr<MBC> make_mbc(const std::vector<u8> &rom)
        {
            if (rom.size() <= 0x149)
            {
                return std::make_unique<RomOnly>(rom, 0);
            }

            u8 type = rom[0x147];
            std::size_t ram = ram_size_from_header(rom[0x149]);

            switch (type)
            {
            case 0x00:
            case 0x08:
                return std::make_unique<RomOnly>(rom, ram);
            case 0x09:
            {
                auto plain = std::make_unique<RomOnly>(rom, ram);
                plain->battery = true;
                return plain;
            }
            case 0x01:
            case 0x02:
                return std::make_unique<MBC1>(rom, ram);
            case 0x03:
            {
                auto mbc1 = std::make_unique<MBC1>(rom, ram);
                mbc1->battery = true;
                return mbc1;
            }
            case 0x0F:
            case 0x10:
            {
                auto mbc3 = std::make_unique<MBC3>(rom, ram);
                mbc3->battery = true;
                mbc3->has_rtc = true;
                return mbc3;
            }
            case 0x11:
            case 0x12:
                return std::make_unique<MBC3>(rom, ram);
            case 0x13:
            {
                auto mbc3 = std::make_unique<MBC3>(rom, ram);
                mbc3->battery = true;
                return mbc3;
            }
            case 0x19:
            case 0x1A:
            case 0x1B:
            case 0x1C:
            case 0x1D:
            case 0x1E:
            {
                auto mbc5 = std::make_unique<MBC5>(rom, ram);
                mbc5->rumble = type >= 0x1C;
                mbc5->battery = type == 0x1B || type == 0x1E;
                return mbc5;
            }
            default:
                fprintf(stderr, "Unsupported cartridge type 0x%02X; running as ROM only\n", type);
                return std::make_unique<RomOnly>(rom, ram);
            }
        }
    }

    Bus::Bus(PPU &ppu, Timer &timer) : ppu(ppu), timer(timer)
    {
        vram.resize(0x2000);
        wram.resize(0x2000);
        oam.resize(0xA0);
        io_regs.resize(0x80);
        hram.resize(0x7F);
    }

    u8 Bus::read(u16 addr) const
    {
        if (addr <= 0x7FFF) // ROM
        {
            if (mbc)
            {
                return mbc->read(addr);
            }
            return (addr < rom.size()) ? rom[addr] : 0xFF;
        }
        if (addr <= 0x9FFF) // VRAM
        {
            return vram[addr - 0x8000];
        }
        if (addr <= 0xBFFF) // external cartridge RAM
        {
            return mbc ? mbc->read(addr) : 0xFF;
        }
        if (addr <= 0xDFFF) // WRAM
        {
            return wram[addr - 0xC000];
        }
        if (addr <= 0xFDFF) // echo RAM -- mirrors 0xC000-0xDDFF
        {
            return wram[addr - 0xE000];
        }
        if (addr <= 0xFE9F) // OAM
        {
            return oam[addr - 0xFE00];
        }
        if (addr <= 0xFEFF) // unusable
        {
            return 0xFF;
        }
        if (addr == 0xFF40) // LCDC
        {
            return ppu.lcdc;
        }
        if (addr == 0xFF41) // STAT
        {
            return ppu.stat_read();
        }
        if (addr == 0xFF42) // SCY
        {
            return ppu.scy;
        }
        if (addr == 0xFF43) // SCX
        {
            return ppu.scx;
        }
        if (addr == 0xFF44) // LY
        {
            return ppu.ly;
        }
        if (addr == 0xFF45) // LYC
        {
            return ppu.lyc;
        }
        if (addr == 0xFF47) // BGP
        {
            return ppu.bgp;
        }
        if (addr == 0xFF48) // SP0
        {
            return ppu.sp0;
        }
        if (addr == 0xFF49) // SP1
        {
            return ppu.sp1;
        }
        if (addr == 0xFF4A) // WY
        {
            return ppu.wy;
        }
        if (addr == 0xFF4B) // WX
        {
            return ppu.wx;
        }
        if (addr == 0xFF00) // joypad
        {
            return joypad.read();
        }
        if (addr == 0xFF04) // DIV -- top byte of the internal counter
        {
            return static_cast<u8>(timer.div >> 8);
        }
        if (addr == 0xFF05) // TIMA
        {
            return static_cast<u8>(timer.tima);
        }
        if (addr == 0xFF06) // TMA
        {
            return static_cast<u8>(timer.tma);
        }
        if (addr == 0xFF07) // TAC -- unused bits read as 1
        {
            return static_cast<u8>(timer.tac | 0xF8);
        }
        if (addr == 0xFF0F) // IF
        {
            return iflag | 0xE0;
        }
        if (addr >= 0xFF10 && addr <= 0xFF3F) // sound registers and wave RAM
        {
            return apu.read(addr);
        }
        if (addr <= 0xFF7F) // I/O registers: unmapped ones read 0xFF, as on a DMG
        {
            return is_stored_io(addr) ? io_regs[addr - 0xFF00] : 0xFF;
        }
        if (addr <= 0xFFFE) // HRAM
        {
            return hram[addr - 0xFF80];
        }
        return ie; // 0xFFFF
    }

    void Bus::write(u16 addr, u8 value)
    {
        if (addr <= 0x7FFF) // ROM range: writes go to the bank controller
        {
            if (mbc)
            {
                mbc->write(addr, value);
            }
            return;
        }
        if (addr <= 0x9FFF) // VRAM
        {
            vram[addr - 0x8000] = value;
            return;
        }
        if (addr <= 0xBFFF) // external cartridge RAM
        {
            if (mbc)
            {
                mbc->write(addr, value);
            }
            return;
        }
        if (addr <= 0xDFFF) // WRAM
        {
            wram[addr - 0xC000] = value;
            return;
        }
        if (addr <= 0xFDFF) // echo RAM -- mirrors 0xC000-0xDDFF
        {
            wram[addr - 0xE000] = value;
            return;
        }
        if (addr <= 0xFE9F) // OAM
        {
            oam[addr - 0xFE00] = value;
            return;
        }
        if (addr <= 0xFEFF) // unusable
        {
            return;
        }
        if (addr == 0xFF02) // SC -- writing the transfer-start bit captures SB immediately
        {
            io_regs[addr - 0xFF00] = value;
            if (value & 0x80)
            {
                serial_output.push_back(io_regs[0xFF01 - 0xFF00]); // SB
                io_regs[addr - 0xFF00] &= 0x7F;                    // no link cable attached -- "finish" the transfer at once
            }
            return;
        }
        if (addr == 0xFF04) // DIV -- writing any value resets it to 0
        {
            timer.div = 0;
            return;
        }
        if (addr == 0xFF05) // TIMA
        {
            timer.tima = value;
            return;
        }
        if (addr == 0xFF06) // TMA
        {
            timer.tma = value;
            return;
        }
        if (addr == 0xFF07) // TAC
        {
            timer.tac = value;
            return;
        }
        if (addr == 0xFF00) // joypad
        {
            joypad.write(value);
            return;
        }
        if (addr == 0xFF0F) // IF
        {
            iflag = value;
            return;
        }
        if (addr == 0xFF40) // LCDC
        {
            ppu.lcdc = value;
            return;
        }
        if (addr == 0xFF41) // STAT
        {
            ppu.stat = value & 0x78;
            return;
        }
        if (addr == 0xFF42) // SCY
        {
            ppu.scy = value;
            return;
        }
        if (addr == 0xFF43) // SCX
        {
            ppu.scx = value;
            return;
        }
        if (addr == 0xFF44) // LY -- writing has no effect
        {
            return;
        }
        if (addr == 0xFF45) // LYC
        {
            ppu.lyc = value;
            return;
        }
        if (addr == 0xFF47) // BGP
        {
            ppu.bgp = value;
            return;
        }
        if (addr == 0xFF48) // SP0
        {
            ppu.sp0 = value;
            return;
        }
        if (addr == 0xFF49) // SP1
        {
            ppu.sp1 = value;
            return;
        }
        if (addr == 0xFF4A) // WY
        {
            ppu.wy = value;
            return;
        }
        if (addr == 0xFF4B) // WX
        {
            ppu.wx = value;
            return;
        }
        if (addr == 0xFF46) // DMA -- copies 160 bytes into OAM
        {
            io_regs[addr - 0xFF00] = value;
            u16 source = static_cast<u16>(value << 8);
            for (int i = 0; i < 0xA0; i++)
            {
                oam[i] = read(static_cast<u16>(source + i));
            }
            return;
        }
        if (addr >= 0xFF10 && addr <= 0xFF3F) // sound registers and wave RAM
        {
            apu.write(addr, value);
            return;
        }
        if (addr <= 0xFF7F) // I/O registers: writes to unmapped ones are ignored, as on a DMG
        {
            if (is_stored_io(addr))
            {
                io_regs[addr - 0xFF00] = value;
            }
            return;
        }
        if (addr <= 0xFFFE) // HRAM
        {
            hram[addr - 0xFF80] = value;
            return;
        }
        ie = value; // 0xFFFF
    }

    bool Bus::load_rom(const std::string &filename)
    {
        FILE *file = fopen(filename.c_str(), "rb");
        if (!file)
        {
            return false;
        }

        fseek(file, 0, SEEK_END);
        long size = ftell(file);
        if (size < 0)
        {
            fclose(file);
            return false;
        }
        fseek(file, 0, SEEK_SET);

        rom.resize(static_cast<size_t>(size));
        size_t read = fread(rom.data(), 1, size, file);
        fclose(file);
        if (read != static_cast<size_t>(size))
        {
            return false;
        }

        mbc = make_mbc(rom);
        return true;
    }

    void Bus::tick(int cycles)
    {
        if (mbc)
        {
            mbc->tick(cycles);
        }
        apu.tick(cycles);
    }

    bool Bus::load_save(const std::string &path)
    {
        if (!mbc || !mbc->battery)
        {
            return false;
        }

        FILE *file = fopen(path.c_str(), "rb");
        if (!file)
        {
            return false;
        }

        fseek(file, 0, SEEK_END);
        long size = ftell(file);
        fseek(file, 0, SEEK_SET);
        if (size < 0)
        {
            fclose(file);
            return false;
        }

        std::vector<u8> data(static_cast<size_t>(size));
        size_t read = fread(data.data(), 1, data.size(), file);
        fclose(file);
        if (read != data.size())
        {
            return false;
        }

        mbc->load_data(data, static_cast<u64>(std::time(nullptr)));
        return true;
    }

    bool Bus::save_ram(const std::string &path) const
    {
        if (!mbc || !mbc->battery)
        {
            return true;
        }

        std::vector<u8> data = mbc->save_data(static_cast<u64>(std::time(nullptr)));
        if (data.empty())
        {
            return true;
        }

        FILE *file = fopen(path.c_str(), "wb");
        if (!file)
        {
            return false;
        }
        size_t written = fwrite(data.data(), 1, data.size(), file);
        fclose(file);
        return written == data.size();
    }
}
