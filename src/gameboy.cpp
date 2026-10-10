#include "gb/gameboy.hpp"

namespace gb {
    bool GameBoy::load_rom(const std::string &filename)
    {
        return bus.load_rom(filename);
    }

    void GameBoy::reset()
    {
        cpu.regs.a = 0x01;
        cpu.regs.f = 0xB0;
        cpu.regs.bc = 0x0013;
        cpu.regs.de = 0x00D8;
        cpu.regs.hl = 0x014D;
        cpu.regs.sp = 0xFFFE;
        cpu.regs.pc = 0x0100;
        ppu.lcdc = 0x91;
        ppu.stat = 0x85;
        ppu.bgp = 0xFC;
        bus.apu.reset();
    }

    int GameBoy::step()
    {
        int cycles = cpu.step();
        bool timer_overflow = timer.tick(cycles);
        if (timer_overflow)
        {
            bus.iflag |= 0x04;
        }
        bus.tick(cycles);
        bool vblank_interrupt = ppu.tick(cycles);
        if (ppu.line_ready)
        {
            ppu.render_line(bus.vram, bus.oam, ppu.ly);
            ppu.line_ready = false;
        }
        if (vblank_interrupt)
        {
            bus.iflag |= 0x01;
        }
        if (bus.joypad.interrupt_requested)
        {
            bus.iflag |= 0x10;
            bus.joypad.interrupt_requested = false;
        }
        if (ppu.stat_irq_requested)
        {
            bus.iflag |= 0x02;
            ppu.stat_irq_requested = false;
        }
        return cycles;
    }
}