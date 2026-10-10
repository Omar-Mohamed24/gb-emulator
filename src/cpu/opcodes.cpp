#include "gb/cpu.hpp"
#include <cstdio>
#include <cstdlib>

namespace gb {

    int CPU::execute(u8 opcode)
    {
        // 0x40-0x7F: LD r,r' grid. Register index encoding (both dest and
        // src) is B,C,D,E,H,L,(HL),A = 0..7. 0x76 is HALT.
        if (opcode >= 0x40 && opcode <= 0x7F && opcode != 0x76)
        {
            int dest = (opcode - 0x40) / 8;
            int src  = (opcode - 0x40) % 8;
            write_r8(dest, read_r8(src));
            return (src == 6 || dest == 6) ? 8 : 4;
        }

        // 0x80-0xBF: ALU A,r grid. Op index 0..7 = ADD,ADC,SUB,SBC,AND,XOR,OR,CP.
        if (opcode >= 0x80 && opcode <= 0xBF)
        {
            int op  = (opcode - 0x80) / 8;
            int src = (opcode - 0x80) % 8;
            alu_op(op, read_r8(src));
            return (src == 6) ? 8 : 4;
        }

        switch (opcode)
        {
        // ---- 0x00-0x3F: misc, 16-bit loads, inc/dec, rotates ----
        case 0x00: // NOP
            return 4;

        case 0x01: // LD BC,nn
            regs.bc = fetch16();
            return 12;

        case 0x02: // LD (BC),A
            bus.write(regs.bc, regs.a);
            return 8;

        case 0x03: // INC BC
            regs.bc++;
            return 8;

        case 0x04: // INC B
        {
            u8 value = regs.b;
            regs.b = static_cast<u8>(value + 1);
            regs.set_flag(Registers::FLAG_Z, regs.b == 0);
            regs.set_flag(Registers::FLAG_N, false);
            regs.set_flag(Registers::FLAG_H, (value & 0x0F) == 0x0F);
            return 4;
        }

        case 0x05: // DEC B
        {
            u8 value = regs.b;
            regs.b = static_cast<u8>(value - 1);
            regs.set_flag(Registers::FLAG_Z, regs.b == 0);
            regs.set_flag(Registers::FLAG_N, true);
            regs.set_flag(Registers::FLAG_H, (value & 0x0F) == 0x00);
            return 4;
        }

        case 0x06: // LD B,n
            regs.b = fetch8();
            return 8;

        case 0x07: // RLCA
        {
            u8 carry = (regs.a & 0x80) ? 1 : 0;
            regs.a = static_cast<u8>((regs.a << 1) | carry);
            regs.set_flag(Registers::FLAG_Z, false);
            regs.set_flag(Registers::FLAG_N, false);
            regs.set_flag(Registers::FLAG_H, false);
            regs.set_flag(Registers::FLAG_C, carry != 0);
            return 4;
        }

        case 0x08: // LD (nn),SP
        {
            u16 addr = fetch16();
            bus.write(addr, static_cast<u8>(regs.sp & 0xFF));
            bus.write(static_cast<u16>(addr + 1), static_cast<u8>(regs.sp >> 8));
            return 20;
        }

        case 0x09: // ADD HL,BC
        {
            u16 hl = regs.hl, rr = regs.bc;
            u32 result = static_cast<u32>(hl) + rr;
            regs.set_flag(Registers::FLAG_N, false);
            regs.set_flag(Registers::FLAG_H, ((hl & 0x0FFF) + (rr & 0x0FFF)) > 0x0FFF);
            regs.set_flag(Registers::FLAG_C, result > 0xFFFF);
            regs.hl = static_cast<u16>(result);
            return 8;
        }

        case 0x0A: // LD A,(BC)
            regs.a = bus.read(regs.bc);
            return 8;

        case 0x0B: // DEC BC
            regs.bc--;
            return 8;

        case 0x0C: // INC C
        {
            u8 value = regs.c;
            regs.c = static_cast<u8>(value + 1);
            regs.set_flag(Registers::FLAG_Z, regs.c == 0);
            regs.set_flag(Registers::FLAG_N, false);
            regs.set_flag(Registers::FLAG_H, (value & 0x0F) == 0x0F);
            return 4;
        }

        case 0x0D: // DEC C
        {
            u8 value = regs.c;
            regs.c = static_cast<u8>(value - 1);
            regs.set_flag(Registers::FLAG_Z, regs.c == 0);
            regs.set_flag(Registers::FLAG_N, true);
            regs.set_flag(Registers::FLAG_H, (value & 0x0F) == 0x00);
            return 4;
        }

        case 0x0E: // LD C,n
            regs.c = fetch8();
            return 8;

        case 0x0F: // RRCA
        {
            u8 carry = regs.a & 0x01;
            regs.a = static_cast<u8>((regs.a >> 1) | (carry << 7));
            regs.set_flag(Registers::FLAG_Z, false);
            regs.set_flag(Registers::FLAG_N, false);
            regs.set_flag(Registers::FLAG_H, false);
            regs.set_flag(Registers::FLAG_C, carry != 0);
            return 4;
        }

        case 0x10: // STOP
            fetch8(); // consume the padding byte
            halted = true;
            return 4;

        case 0x11: // LD DE,nn
            regs.de = fetch16();
            return 12;

        case 0x12: // LD (DE),A
            bus.write(regs.de, regs.a);
            return 8;

        case 0x13: // INC DE
            regs.de++;
            return 8;

        case 0x14: // INC D
        {
            u8 value = regs.d;
            regs.d = static_cast<u8>(value + 1);
            regs.set_flag(Registers::FLAG_Z, regs.d == 0);
            regs.set_flag(Registers::FLAG_N, false);
            regs.set_flag(Registers::FLAG_H, (value & 0x0F) == 0x0F);
            return 4;
        }

        case 0x15: // DEC D
        {
            u8 value = regs.d;
            regs.d = static_cast<u8>(value - 1);
            regs.set_flag(Registers::FLAG_Z, regs.d == 0);
            regs.set_flag(Registers::FLAG_N, true);
            regs.set_flag(Registers::FLAG_H, (value & 0x0F) == 0x00);
            return 4;
        }

        case 0x16: // LD D,n
            regs.d = fetch8();
            return 8;

        case 0x17: // RLA
        {
            u8 old_carry = regs.flag(Registers::FLAG_C) ? 1 : 0;
            u8 new_carry = (regs.a & 0x80) ? 1 : 0;
            regs.a = static_cast<u8>((regs.a << 1) | old_carry);
            regs.set_flag(Registers::FLAG_Z, false);
            regs.set_flag(Registers::FLAG_N, false);
            regs.set_flag(Registers::FLAG_H, false);
            regs.set_flag(Registers::FLAG_C, new_carry != 0);
            return 4;
        }

        case 0x18: // JR e
        {
            i8 offset = static_cast<i8>(fetch8());
            regs.pc = static_cast<u16>(regs.pc + offset);
            return 12;
        }

        case 0x19: // ADD HL,DE
        {
            u16 hl = regs.hl, rr = regs.de;
            u32 result = static_cast<u32>(hl) + rr;
            regs.set_flag(Registers::FLAG_N, false);
            regs.set_flag(Registers::FLAG_H, ((hl & 0x0FFF) + (rr & 0x0FFF)) > 0x0FFF);
            regs.set_flag(Registers::FLAG_C, result > 0xFFFF);
            regs.hl = static_cast<u16>(result);
            return 8;
        }

        case 0x1A: // LD A,(DE)
            regs.a = bus.read(regs.de);
            return 8;

        case 0x1B: // DEC DE
            regs.de--;
            return 8;

        case 0x1C: // INC E
        {
            u8 value = regs.e;
            regs.e = static_cast<u8>(value + 1);
            regs.set_flag(Registers::FLAG_Z, regs.e == 0);
            regs.set_flag(Registers::FLAG_N, false);
            regs.set_flag(Registers::FLAG_H, (value & 0x0F) == 0x0F);
            return 4;
        }

        case 0x1D: // DEC E
        {
            u8 value = regs.e;
            regs.e = static_cast<u8>(value - 1);
            regs.set_flag(Registers::FLAG_Z, regs.e == 0);
            regs.set_flag(Registers::FLAG_N, true);
            regs.set_flag(Registers::FLAG_H, (value & 0x0F) == 0x00);
            return 4;
        }

        case 0x1E: // LD E,n
            regs.e = fetch8();
            return 8;

        case 0x1F: // RRA
        {
            u8 old_carry = regs.flag(Registers::FLAG_C) ? 1 : 0;
            u8 new_carry = regs.a & 0x01;
            regs.a = static_cast<u8>((regs.a >> 1) | (old_carry << 7));
            regs.set_flag(Registers::FLAG_Z, false);
            regs.set_flag(Registers::FLAG_N, false);
            regs.set_flag(Registers::FLAG_H, false);
            regs.set_flag(Registers::FLAG_C, new_carry != 0);
            return 4;
        }

        case 0x20: // JR NZ,e
        {
            i8 offset = static_cast<i8>(fetch8());
            if (!regs.flag(Registers::FLAG_Z))
            {
                regs.pc = static_cast<u16>(regs.pc + offset);
                return 12;
            }
            return 8;
        }

        case 0x21: // LD HL,nn
            regs.hl = fetch16();
            return 12;

        case 0x22: // LD (HL+),A
            bus.write(regs.hl, regs.a);
            regs.hl++;
            return 8;

        case 0x23: // INC HL
            regs.hl++;
            return 8;

        case 0x24: // INC H
        {
            u8 value = regs.h;
            regs.h = static_cast<u8>(value + 1);
            regs.set_flag(Registers::FLAG_Z, regs.h == 0);
            regs.set_flag(Registers::FLAG_N, false);
            regs.set_flag(Registers::FLAG_H, (value & 0x0F) == 0x0F);
            return 4;
        }

        case 0x25: // DEC H
        {
            u8 value = regs.h;
            regs.h = static_cast<u8>(value - 1);
            regs.set_flag(Registers::FLAG_Z, regs.h == 0);
            regs.set_flag(Registers::FLAG_N, true);
            regs.set_flag(Registers::FLAG_H, (value & 0x0F) == 0x00);
            return 4;
        }

        case 0x26: // LD H,n
            regs.h = fetch8();
            return 8;

        case 0x27: // DAA
        {
            int a = regs.a;
            u8 correction = 0;
            bool carry = regs.flag(Registers::FLAG_C);

            if (regs.flag(Registers::FLAG_H) || (!regs.flag(Registers::FLAG_N) && (a & 0x0F) > 9))
                correction |= 0x06;

            if (regs.flag(Registers::FLAG_C) || (!regs.flag(Registers::FLAG_N) && a > 0x99))
            {
                correction |= 0x60;
                carry = true;
            }

            a += regs.flag(Registers::FLAG_N) ? -correction : correction;
            regs.a = static_cast<u8>(a);

            regs.set_flag(Registers::FLAG_Z, regs.a == 0);
            regs.set_flag(Registers::FLAG_H, false);
            regs.set_flag(Registers::FLAG_C, carry);
            return 4;
        }

        case 0x28: // JR Z,e
        {
            i8 offset = static_cast<i8>(fetch8());
            if (regs.flag(Registers::FLAG_Z))
            {
                regs.pc = static_cast<u16>(regs.pc + offset);
                return 12;
            }
            return 8;
        }

        case 0x29: // ADD HL,HL
        {
            u16 hl = regs.hl;
            u32 result = static_cast<u32>(hl) + hl;
            regs.set_flag(Registers::FLAG_N, false);
            regs.set_flag(Registers::FLAG_H, ((hl & 0x0FFF) + (hl & 0x0FFF)) > 0x0FFF);
            regs.set_flag(Registers::FLAG_C, result > 0xFFFF);
            regs.hl = static_cast<u16>(result);
            return 8;
        }

        case 0x2A: // LD A,(HL+)
            regs.a = bus.read(regs.hl);
            regs.hl++;
            return 8;

        case 0x2B: // DEC HL
            regs.hl--;
            return 8;

        case 0x2C: // INC L
        {
            u8 value = regs.l;
            regs.l = static_cast<u8>(value + 1);
            regs.set_flag(Registers::FLAG_Z, regs.l == 0);
            regs.set_flag(Registers::FLAG_N, false);
            regs.set_flag(Registers::FLAG_H, (value & 0x0F) == 0x0F);
            return 4;
        }

        case 0x2D: // DEC L
        {
            u8 value = regs.l;
            regs.l = static_cast<u8>(value - 1);
            regs.set_flag(Registers::FLAG_Z, regs.l == 0);
            regs.set_flag(Registers::FLAG_N, true);
            regs.set_flag(Registers::FLAG_H, (value & 0x0F) == 0x00);
            return 4;
        }

        case 0x2E: // LD L,n
            regs.l = fetch8();
            return 8;

        case 0x2F: // CPL
            regs.a = static_cast<u8>(~regs.a);
            regs.set_flag(Registers::FLAG_N, true);
            regs.set_flag(Registers::FLAG_H, true);
            return 4;

        case 0x30: // JR NC,e
        {
            i8 offset = static_cast<i8>(fetch8());
            if (!regs.flag(Registers::FLAG_C))
            {
                regs.pc = static_cast<u16>(regs.pc + offset);
                return 12;
            }
            return 8;
        }

        case 0x31: // LD SP,nn
            regs.sp = fetch16();
            return 12;

        case 0x32: // LD (HL-),A
            bus.write(regs.hl, regs.a);
            regs.hl--;
            return 8;

        case 0x33: // INC SP
            regs.sp++;
            return 8;

        case 0x34: // INC (HL)
        {
            u8 value = bus.read(regs.hl);
            u8 result = static_cast<u8>(value + 1);
            bus.write(regs.hl, result);
            regs.set_flag(Registers::FLAG_Z, result == 0);
            regs.set_flag(Registers::FLAG_N, false);
            regs.set_flag(Registers::FLAG_H, (value & 0x0F) == 0x0F);
            return 12;
        }

        case 0x35: // DEC (HL)
        {
            u8 value = bus.read(regs.hl);
            u8 result = static_cast<u8>(value - 1);
            bus.write(regs.hl, result);
            regs.set_flag(Registers::FLAG_Z, result == 0);
            regs.set_flag(Registers::FLAG_N, true);
            regs.set_flag(Registers::FLAG_H, (value & 0x0F) == 0x00);
            return 12;
        }

        case 0x36: // LD (HL),n
            bus.write(regs.hl, fetch8());
            return 12;

        case 0x37: // SCF
            regs.set_flag(Registers::FLAG_N, false);
            regs.set_flag(Registers::FLAG_H, false);
            regs.set_flag(Registers::FLAG_C, true);
            return 4;

        case 0x38: // JR C,e
        {
            i8 offset = static_cast<i8>(fetch8());
            if (regs.flag(Registers::FLAG_C))
            {
                regs.pc = static_cast<u16>(regs.pc + offset);
                return 12;
            }
            return 8;
        }

        case 0x39: // ADD HL,SP
        {
            u16 hl = regs.hl, rr = regs.sp;
            u32 result = static_cast<u32>(hl) + rr;
            regs.set_flag(Registers::FLAG_N, false);
            regs.set_flag(Registers::FLAG_H, ((hl & 0x0FFF) + (rr & 0x0FFF)) > 0x0FFF);
            regs.set_flag(Registers::FLAG_C, result > 0xFFFF);
            regs.hl = static_cast<u16>(result);
            return 8;
        }

        case 0x3A: // LD A,(HL-)
            regs.a = bus.read(regs.hl);
            regs.hl--;
            return 8;

        case 0x3B: // DEC SP
            regs.sp--;
            return 8;

        case 0x3C: // INC A
        {
            u8 value = regs.a;
            regs.a = static_cast<u8>(value + 1);
            regs.set_flag(Registers::FLAG_Z, regs.a == 0);
            regs.set_flag(Registers::FLAG_N, false);
            regs.set_flag(Registers::FLAG_H, (value & 0x0F) == 0x0F);
            return 4;
        }

        case 0x3D: // DEC A
        {
            u8 value = regs.a;
            regs.a = static_cast<u8>(value - 1);
            regs.set_flag(Registers::FLAG_Z, regs.a == 0);
            regs.set_flag(Registers::FLAG_N, true);
            regs.set_flag(Registers::FLAG_H, (value & 0x0F) == 0x00);
            return 4;
        }

        case 0x3E: // LD A,n
            regs.a = fetch8();
            return 8;

        case 0x3F: // CCF
            regs.set_flag(Registers::FLAG_N, false);
            regs.set_flag(Registers::FLAG_H, false);
            regs.set_flag(Registers::FLAG_C, !regs.flag(Registers::FLAG_C));
            return 4;

        case 0x76: // HALT
            halted = true;
            return 4;

        // ---- 0xC0-0xFF: control flow, stack, I/O, misc ----
        case 0xC0: // RET NZ
            if (!regs.flag(Registers::FLAG_Z)) { regs.pc = pop16(); return 20; }
            return 8;

        case 0xC1: // POP BC
            regs.bc = pop16();
            return 12;

        case 0xC2: // JP NZ,nn
        {
            u16 addr = fetch16();
            if (!regs.flag(Registers::FLAG_Z)) { regs.pc = addr; return 16; }
            return 12;
        }

        case 0xC3: // JP nn
            regs.pc = fetch16();
            return 16;

        case 0xC4: // CALL NZ,nn
        {
            u16 addr = fetch16();
            if (!regs.flag(Registers::FLAG_Z)) { push16(regs.pc); regs.pc = addr; return 24; }
            return 12;
        }

        case 0xC5: // PUSH BC
            push16(regs.bc);
            return 16;

        case 0xC6: // ADD A,n
            alu_op(0, fetch8());
            return 8;

        case 0xC7: // RST 00H
            push16(regs.pc);
            regs.pc = 0x0000;
            return 16;

        case 0xC8: // RET Z
            if (regs.flag(Registers::FLAG_Z)) { regs.pc = pop16(); return 20; }
            return 8;

        case 0xC9: // RET
            regs.pc = pop16();
            return 16;

        case 0xCA: // JP Z,nn
        {
            u16 addr = fetch16();
            if (regs.flag(Registers::FLAG_Z)) { regs.pc = addr; return 16; }
            return 12;
        }

        case 0xCC: // CALL Z,nn
        {
            u16 addr = fetch16();
            if (regs.flag(Registers::FLAG_Z)) { push16(regs.pc); regs.pc = addr; return 24; }
            return 12;
        }

        case 0xCD: // CALL nn
        {
            u16 addr = fetch16();
            push16(regs.pc);
            regs.pc = addr;
            return 24;
        }

        case 0xCE: // ADC A,n
            alu_op(1, fetch8());
            return 8;

        case 0xCF: // RST 08H
            push16(regs.pc);
            regs.pc = 0x0008;
            return 16;

        case 0xD0: // RET NC
            if (!regs.flag(Registers::FLAG_C)) { regs.pc = pop16(); return 20; }
            return 8;

        case 0xD1: // POP DE
            regs.de = pop16();
            return 12;

        case 0xD2: // JP NC,nn
        {
            u16 addr = fetch16();
            if (!regs.flag(Registers::FLAG_C)) { regs.pc = addr; return 16; }
            return 12;
        }

        case 0xD4: // CALL NC,nn
        {
            u16 addr = fetch16();
            if (!regs.flag(Registers::FLAG_C)) { push16(regs.pc); regs.pc = addr; return 24; }
            return 12;
        }

        case 0xD5: // PUSH DE
            push16(regs.de);
            return 16;

        case 0xD6: // SUB n
            alu_op(2, fetch8());
            return 8;

        case 0xD7: // RST 10H
            push16(regs.pc);
            regs.pc = 0x0010;
            return 16;

        case 0xD8: // RET C
            if (regs.flag(Registers::FLAG_C)) { regs.pc = pop16(); return 20; }
            return 8;

        case 0xD9: // RETI
            regs.pc = pop16();
            ime = true;
            return 16;

        case 0xDA: // JP C,nn
        {
            u16 addr = fetch16();
            if (regs.flag(Registers::FLAG_C)) { regs.pc = addr; return 16; }
            return 12;
        }

        case 0xDC: // CALL C,nn
        {
            u16 addr = fetch16();
            if (regs.flag(Registers::FLAG_C)) { push16(regs.pc); regs.pc = addr; return 24; }
            return 12;
        }

        case 0xDE: // SBC A,n
            alu_op(3, fetch8());
            return 8;

        case 0xDF: // RST 18H
            push16(regs.pc);
            regs.pc = 0x0018;
            return 16;

        case 0xE0: // LDH (n),A
            bus.write(static_cast<u16>(0xFF00 + fetch8()), regs.a);
            return 12;

        case 0xE1: // POP HL
            regs.hl = pop16();
            return 12;

        case 0xE2: // LDH (C),A
            bus.write(static_cast<u16>(0xFF00 + regs.c), regs.a);
            return 8;

        case 0xE5: // PUSH HL
            push16(regs.hl);
            return 16;

        case 0xE6: // AND n
            alu_op(4, fetch8());
            return 8;

        case 0xE7: // RST 20H
            push16(regs.pc);
            regs.pc = 0x0020;
            return 16;

        case 0xE8: // ADD SP,e
        {
            i8 offset = static_cast<i8>(fetch8());
            u16 sp = regs.sp;
            u8 low = static_cast<u8>(offset);
            bool h = ((sp & 0x0F) + (low & 0x0F)) > 0x0F;
            bool c = ((sp & 0xFF) + low) > 0xFF;
            regs.sp = static_cast<u16>(sp + offset);
            regs.set_flag(Registers::FLAG_Z, false);
            regs.set_flag(Registers::FLAG_N, false);
            regs.set_flag(Registers::FLAG_H, h);
            regs.set_flag(Registers::FLAG_C, c);
            return 16;
        }

        case 0xE9: // JP (HL) -- jumps to the address held in HL, does not read memory at HL
            regs.pc = regs.hl;
            return 4;

        case 0xEA: // LD (nn),A
            bus.write(fetch16(), regs.a);
            return 16;

        case 0xEE: // XOR n
            alu_op(5, fetch8());
            return 8;

        case 0xEF: // RST 28H
            push16(regs.pc);
            regs.pc = 0x0028;
            return 16;

        case 0xF0: // LDH A,(n)
            regs.a = bus.read(static_cast<u16>(0xFF00 + fetch8()));
            return 12;

        case 0xF1: // POP AF -- low nibble of F is always 0 on real hardware
            regs.af = pop16() & 0xFFF0;
            return 12;

        case 0xF2: // LDH A,(C)
            regs.a = bus.read(static_cast<u16>(0xFF00 + regs.c));
            return 8;

        case 0xF3: // DI
            ime = false;
            return 4;

        case 0xF5: // PUSH AF
            push16(regs.af);
            return 16;

        case 0xF6: // OR n
            alu_op(6, fetch8());
            return 8;

        case 0xF7: // RST 30H
            push16(regs.pc);
            regs.pc = 0x0030;
            return 16;

        case 0xF8: // LD HL,SP+e
        {
            i8 offset = static_cast<i8>(fetch8());
            u16 sp = regs.sp;
            u8 low = static_cast<u8>(offset);
            bool h = ((sp & 0x0F) + (low & 0x0F)) > 0x0F;
            bool c = ((sp & 0xFF) + low) > 0xFF;
            regs.hl = static_cast<u16>(sp + offset);
            regs.set_flag(Registers::FLAG_Z, false);
            regs.set_flag(Registers::FLAG_N, false);
            regs.set_flag(Registers::FLAG_H, h);
            regs.set_flag(Registers::FLAG_C, c);
            return 12;
        }

        case 0xF9: // LD SP,HL
            regs.sp = regs.hl;
            return 8;

        case 0xFA: // LD A,(nn)
            regs.a = bus.read(fetch16());
            return 16;

        case 0xFB: // EI
            ime_delay = 2;
            return 4;

        case 0xFE: // CP n
            alu_op(7, fetch8());
            return 8;

        case 0xFF: // RST 38H
            push16(regs.pc);
            regs.pc = 0x0038;
            return 16;

        default:
            fprintf(stderr, "Unknown opcode: 0x%02X at PC=0x%04X\n", opcode, static_cast<unsigned>(regs.pc - 1));
            abort();
        }
    }

}
