#include "gb/gameboy.hpp"
#include "gb/registers.hpp"
#include "gb/mbc/mbc1.hpp"
#include "gb/mbc/mbc5.hpp"
#include <cstdio>
#include <vector>

using namespace gb;

namespace
{

    int failures = 0;

    void check(bool condition, const char *description)
    {
        if (condition)
        {
            printf("[PASS] %s\n", description);
        }
        else
        {
            printf("[FAIL] %s\n", description);
            failures++;
        }
    }

    void load_program(GameBoy &gb, std::vector<u8> program)
    {
        gb.bus.rom = std::move(program);
    }

}

int main()
{
    // ---- Registers (Milestone 1) ----
    {
        Registers regs;
        regs.bc = 0x1234;
        check(regs.b == 0x12, "BC's high byte aliases B");
        check(regs.c == 0x34, "BC's low byte aliases C");
    }
    {
        Registers regs;
        regs.set_flag(Registers::FLAG_Z, true);
        regs.set_flag(Registers::FLAG_C, true);
        check(regs.flag(Registers::FLAG_Z), "set_flag/flag round-trip for Z");
        check(regs.flag(Registers::FLAG_C), "set_flag/flag round-trip for C");
        check(!regs.flag(Registers::FLAG_N), "flags that were never set stay clear");
        check((regs.f & 0x0F) == 0, "the low nibble of F is always masked to 0");
    }

    // ---- Bus memory map (Milestone 3.5) ----
    {
        GameBoy gb;
        gb.bus.write(0xC010, 0x42);
        check(gb.bus.read(0xC010) == 0x42, "WRAM write/read round-trip");
        check(gb.bus.read(0xE010) == 0x42, "echo RAM mirrors WRAM on read");
        gb.bus.write(0xE020, 0x99);
        check(gb.bus.read(0xC020) == 0x99, "writing through echo RAM lands in WRAM");
    }
    {
        GameBoy gb;
        gb.bus.write(0xFF90, 0x7A);
        check(gb.bus.read(0xFF90) == 0x7A, "HRAM write/read round-trip");
    }
    {
        GameBoy gb;
        check(gb.bus.read(0xFF44) == 0x00, "LY always reads 0x00 on reset");
        gb.bus.write(0xFF44, 0x02);
        check(gb.bus.read(0xFF44) == 0x00, "writing LY has no effect");
    }
    {
        GameBoy gb;
        gb.bus.write(0xFFFF, 0x1F);
        check(gb.bus.read(0xFFFF) == 0x1F, "IE write/read round-trip");
    }
    {
        GameBoy gb;
        gb.bus.write(0xFF0F, 0x05);
        check(gb.bus.read(0xFF0F) == 0xE5, "reading IF forces the 3 unwired top bits to 1");
    }
    {
        GameBoy gb;
        gb.bus.write(0xFF01, 'H');  // SB
        gb.bus.write(0xFF02, 0x81); // SC: start transfer
        check(gb.bus.serial_output.size() == 1 && gb.bus.serial_output[0] == 'H',
              "writing SC with bit 7 set captures SB into serial_output");
        check((gb.bus.read(0xFF02) & 0x80) == 0, "SC's start bit clears right away (no real link cable)");
    }

    // ---- CPU opcodes (Milestone 3), now via GameBoy's owned Bus+CPU ----
    {
        GameBoy gb;
        load_program(gb, {0x06, 0x05, 0x48}); // LD B,5 ; LD C,B
        gb.cpu.step();
        gb.cpu.step();
        check(gb.cpu.regs.b == 5, "LD B,n loads the immediate into B");
        check(gb.cpu.regs.c == 5, "LD C,B copies B into C");
    }
    {
        GameBoy gb;
        load_program(gb, {0x3E, 0x0F, 0x06, 0x01, 0x80}); // LD A,0x0F ; LD B,1 ; ADD A,B
        gb.cpu.step();
        gb.cpu.step();
        gb.cpu.step();
        check(gb.cpu.regs.a == 0x10, "ADD A,B computes 0x0F + 0x01");
        check(!gb.cpu.regs.flag(Registers::FLAG_Z), "ADD A,B: result is non-zero");
        check(gb.cpu.regs.flag(Registers::FLAG_H), "ADD A,B: half-carry set crossing the nibble boundary");
        check(!gb.cpu.regs.flag(Registers::FLAG_C), "ADD A,B: no full carry");
    }
    {
        GameBoy gb;
        load_program(gb, {0x06, 0xFF, 0x04}); // LD B,0xFF ; INC B
        gb.cpu.step();
        gb.cpu.step();
        check(gb.cpu.regs.b == 0x00, "INC B wraps 0xFF to 0x00");
        check(gb.cpu.regs.flag(Registers::FLAG_Z), "INC B: zero flag set on wrap");
        check(gb.cpu.regs.flag(Registers::FLAG_H), "INC B: half-carry set on wrap");
        check(!gb.cpu.regs.flag(Registers::FLAG_N), "INC B: N flag always clear");
    }
    {
        GameBoy gb;
        load_program(gb, {0x3E, 0x01, 0x3D, 0x20, 0x02, 0x00, 0x00});
        // LD A,1 ; DEC A (-> A=0, Z=1) ; JR NZ,+2 (not taken) ; NOP ; NOP
        gb.cpu.step();              // LD A,1
        gb.cpu.step();              // DEC A
        int cycles = gb.cpu.step(); // JR NZ,+2
        check(gb.cpu.regs.pc == 5, "JR NZ,e not taken lands right after the operand byte");
        check(cycles == 8, "JR NZ,e not taken costs 8 cycles");
    }
    {
        GameBoy gb;
        load_program(gb, {0x26, 0x80, 0xCB, 0x7C}); // LD H,0x80 ; BIT 7,H
        gb.cpu.step();
        gb.cpu.step();
        check(!gb.cpu.regs.flag(Registers::FLAG_Z), "BIT 7,H: bit 7 of 0x80 is set, so Z is cleared");
        check(gb.cpu.regs.flag(Registers::FLAG_H), "BIT always sets the half-carry flag");
        check(!gb.cpu.regs.flag(Registers::FLAG_N), "BIT always clears the subtract flag");
    }
    {
        GameBoy gb;
        load_program(gb, {0xCD, 0x34, 0x12}); // CALL 0x1234
        gb.cpu.regs.sp = 0xFFFE;
        gb.cpu.step();
        check(gb.cpu.regs.pc == 0x1234, "CALL nn jumps to the target address");
        check(gb.cpu.regs.sp == 0xFFFC, "CALL nn pushes a return address, moving SP down by 2");
    }
    {
        // Bus::write is real now (Milestone 3.5), so this can actually check
        // the round-trip through memory, not just the SP/PC side effects.
        GameBoy gb;
        load_program(gb, {0x01, 0x34, 0x12, 0xC5, 0xD1}); // LD BC,0x1234 ; PUSH BC ; POP DE
        gb.cpu.regs.sp = 0xFFFE;
        gb.cpu.step(); // LD BC,0x1234
        gb.cpu.step(); // PUSH BC
        gb.cpu.step(); // POP DE
        check(gb.cpu.regs.de == 0x1234, "PUSH BC then POP DE round-trips through real stack memory");
        check(gb.cpu.regs.sp == 0xFFFE, "the stack pointer returns to its starting value");
    }

    // ---- Timer (Milestone 5) ----
    {
        Timer timer;
        timer.tac = 0x05; // enabled, fastest rate: 16 cycles per TIMA increment
        timer.tick(15);
        check(timer.tima == 0, "TIMA doesn't increment before the configured rate elapses");
        timer.tick(1); // 16 total now
        check(timer.tima == 1, "TIMA increments once the rate elapses");
    }
    {
        Timer timer;
        timer.tac = 0x05;
        timer.tima = 0xFF;
        timer.tma = 0x10;
        bool overflowed = timer.tick(16);
        check(overflowed, "tick() reports true on TIMA overflow");
        check(timer.tima == 0x10, "TIMA reloads from TMA after overflow");
    }
    {
        Timer timer;
        timer.tac = 0x00; // enable bit clear
        timer.tima = 0x05;
        timer.tick(10000);
        check(timer.tima == 0x05, "TIMA doesn't move at all while TAC's enable bit is clear");
    }
    {
        GameBoy gb;
        gb.timer.div = 1234;        // direct field access, just to simulate elapsed time for this test
        gb.bus.write(0xFF04, 0x99); // any write should reset DIV, regardless of the value
        check(gb.timer.div == 0, "writing any value to DIV resets it to 0");
    }

    // ---- Interrupts (Milestone 5) ----
    {
        // IE and IF both have the timer bit set, IME is on: should dispatch.
        GameBoy gb;
        load_program(gb, {0x00}); // NOP -- never reached, dispatch preempts it
        gb.cpu.regs.sp = 0xFFFE;
        gb.cpu.ime = true;
        gb.bus.write(0xFFFF, 0x04); // IE: timer bit
        gb.bus.write(0xFF0F, 0x04); // IF: timer bit pending
        int cycles = gb.cpu.step();
        check(gb.cpu.regs.pc == 0x0050, "a pending timer interrupt dispatches to its vector (0x0050)");
        check(gb.cpu.regs.sp == 0xFFFC, "dispatch pushes a return address, moving SP down by 2");
        check(!gb.cpu.ime, "dispatch clears IME");
        check((gb.bus.read(0xFF0F) & 0x04) == 0, "dispatch clears the serviced bit in IF");
        check(cycles == 20, "dispatch costs 20 cycles");
    }
    {
        // HALT with nothing pending: CPU idles, no instruction runs, PC doesn't move.
        GameBoy gb;
        load_program(gb, {0x76, 0x3E, 0x99}); // HALT ; LD A,0x99 (must not run while halted)
        gb.cpu.step();                        // HALT
        check(gb.cpu.halted, "HALT sets the halted flag");
        u16 pc_before = gb.cpu.regs.pc;
        int cycles = gb.cpu.step();
        check(gb.cpu.regs.pc == pc_before, "a halted CPU with nothing pending doesn't advance PC");
        check(cycles == 4, "an idle halted step costs 4 cycles");
        check(gb.cpu.regs.a != 0x99, "the instruction after HALT doesn't run while still halted");
    }
    {
        // HALT wakes on a pending interrupt even with IME off, but doesn't dispatch.
        GameBoy gb;
        load_program(gb, {0x76, 0x3E, 0x99}); // HALT ; LD A,0x99
        gb.cpu.ime = false;
        gb.cpu.step(); // HALT
        gb.bus.write(0xFFFF, 0x04);
        gb.bus.write(0xFF0F, 0x04); // pending timer interrupt, IME off
        gb.cpu.step();              // should wake and resume, not dispatch
        check(!gb.cpu.halted, "a pending interrupt wakes the CPU even with IME off");
        check(gb.cpu.regs.a == 0x99, "with IME off, the CPU resumes the next instruction instead of dispatching");
    }
    {
        // EI's enable is delayed by one instruction: an interrupt already
        // pending when EI runs must not preempt the instruction right after EI.
        GameBoy gb;
        load_program(gb, {0xFB, 0x3E, 0x99}); // EI ; LD A,0x99
        gb.cpu.ime = false;
        gb.bus.write(0xFFFF, 0x04);
        gb.bus.write(0xFF0F, 0x04); // already pending before EI even runs
        gb.cpu.step();              // EI
        gb.cpu.step();              // should be LD A,0x99, not a dispatch
        check(gb.cpu.regs.a == 0x99, "EI's delay: the instruction right after EI still runs before any dispatch");
    }

    // ---- Joypad ----
    {
        GameBoy gb;
        gb.bus.write(0xFF00, 0x10); // select the action buttons
        check((gb.bus.read(0xFF00) & 0x0F) == 0x0F, "no button pressed reads as all 1s");
        gb.bus.joypad.set_button(Joypad::A, true);
        check((gb.bus.read(0xFF00) & 0x01) == 0, "pressing A reads as 0 while the action group is selected");
        check(gb.bus.joypad.interrupt_requested, "pressing a selected button requests the joypad interrupt");
        gb.bus.joypad.set_button(Joypad::A, false);
        check((gb.bus.read(0xFF00) & 0x01) == 1, "releasing A reads as 1 again");
    }
    {
        GameBoy gb;
        gb.bus.write(0xFF00, 0x10); // action group selected, directions not
        gb.bus.joypad.set_button(Joypad::RIGHT, true);
        check((gb.bus.read(0xFF00) & 0x0F) == 0x0F, "a direction press is hidden while the action group is selected");
        check(!gb.bus.joypad.interrupt_requested, "a hidden press does not request the joypad interrupt");
    }

    // ---- Timer registers read back ----
    {
        GameBoy gb;
        gb.timer.div = 0x1234;
        gb.timer.tima = 0x12;
        gb.timer.tma = 0x34;
        gb.timer.tac = 0x05;
        check(gb.bus.read(0xFF04) == 0x12, "DIV reads the top byte of the internal counter");
        check(gb.bus.read(0xFF05) == 0x12, "TIMA reads back the timer value");
        check(gb.bus.read(0xFF06) == 0x34, "TMA reads back the modulo value");
        check(gb.bus.read(0xFF07) == 0xFD, "TAC reads back with its unused bits set");
    }

    // ---- Unmapped I/O reads 0xFF and ignores writes ----
    {
        GameBoy gb;
        gb.bus.write(0xFF4D, 0x01); // KEY1 is a CGB register; it does not exist on a DMG
        check(gb.bus.read(0xFF4D) == 0xFF, "an unmapped I/O register reads 0xFF, not the last write");
        gb.bus.write(0xFF03, 0x55);
        check(gb.bus.read(0xFF03) == 0xFF, "a write to an unmapped I/O register is ignored");
        gb.bus.write(0xFF46, 0xC0);
        check(gb.bus.read(0xFF46) == 0xC0, "the DMA register still reads back what was written");
    }

    // ---- Bank controllers ----
    {
        std::vector<u8> rom(0x80000, 0); // 32 banks; the first byte of each bank holds its number
        for (int bank = 0; bank < 32; bank++)
        {
            rom[bank * 0x4000] = static_cast<u8>(bank);
        }
        MBC1 mbc(rom, 0x8000);
        mbc.write(0x2000, 3);
        check(mbc.read(0x4000) == 3, "MBC1 switches the upper ROM bank");
        mbc.write(0x2000, 0);
        check(mbc.read(0x4000) == 1, "MBC1 treats bank 0 as bank 1 in the upper region");
    }
    {
        std::vector<u8> rom(0x80000, 0);
        for (int bank = 0; bank < 32; bank++)
        {
            rom[bank * 0x4000] = static_cast<u8>(bank);
        }
        MBC5 mbc(rom, 0x8000);
        mbc.write(0x2000, 0);
        check(mbc.read(0x4000) == 0, "MBC5 can select bank 0 in the upper region");
        mbc.write(0x2000, 5);
        check(mbc.read(0x4000) == 5, "MBC5 switches the upper ROM bank");
    }
    {
        std::vector<u8> rom(0x80000, 0);
        MBC1 mbc(rom, 0x2000);
        check(mbc.read(0xA000) == 0xFF, "external RAM reads 0xFF while disabled");
        mbc.write(0x0000, 0x0A);
        mbc.write(0xA000, 0x55);
        check(mbc.read(0xA000) == 0x55, "external RAM stores data once enabled");
    }

    {
        std::vector<u8> rom(0x80000, 0);
        MBC5 mbc(rom, 0x20000);
        mbc.rumble = true;
        mbc.write(0x0000, 0x0A);
        mbc.write(0x4000, 0x0B); // bit 3 is the motor; the RAM bank is 3
        mbc.write(0xA000, 0x77);
        mbc.write(0x4000, 0x03);
        check(mbc.read(0xA000) == 0x77, "rumble cartridges use only the low three bits of the RAM bank");
    }

    // ---- DMA copies a block into the sprite table ----
    {
        GameBoy gb;
        for (int i = 0; i < 0xA0; i++)
        {
            gb.bus.write(static_cast<u16>(0xC000 + i), static_cast<u8>(i + 1));
        }
        gb.bus.write(0xFF46, 0xC0);
        check(gb.bus.oam[0] == 1 && gb.bus.oam[0x9F] == 0xA0, "a DMA write copies 160 bytes from the source page into OAM");
    }

    if (failures == 0)
    {
        printf("\nAll tests passed.\n");
    }
    else
    {
        printf("\n%d test(s) failed.\n", failures);
    }

    return failures == 0 ? 0 : 1;
}
