#include "gb/cpu.hpp"
#include "gb/types.hpp"
#include <cstdio>
#include <cstdlib>
using namespace std;

namespace gb
{

    int CPU::step()
    {
        if (ime_delay > 0)
        {
            ime_delay--;
            if (ime_delay == 0)
            {
                ime = true;
            }
        }

        u8 pending = bus.ie & bus.iflag & 0x1F; // only bits 0-4 are valid
        if (halted)
        {
            if (pending)
            {
                halted = false;
            }
            else
            {
                return 4;
            }
        }

        if (ime && pending)
        {
            ime = false;
            bus.iflag &= ~pending;
            push16(regs.pc);

            if (pending & 0x01)
            {
                regs.pc = 0x40; // VBlank
            }
            else if (pending & 0x02)
            {
                regs.pc = 0x48; // LCD STAT
            }
            else if (pending & 0x04)
            {
                regs.pc = 0x50; // Timer
            }
            else if (pending & 0x08)
            {
                regs.pc = 0x58; // Serial
            }
            else if (pending & 0x10)
            {
                regs.pc = 0x60; // Joypad
            }
            return 20;
        }

        u8 opcode = fetch8();
        int cycles = 0;
        if (opcode == 0xCB)
        {
            u8 cb_opcode = fetch8();
            cycles = execute_cb(cb_opcode);
        }
        else
        {
            cycles = execute(opcode);
        }

        return cycles;
    }

    u8 CPU::fetch8()
    {
        u8 value = bus.read(regs.pc);
        regs.pc++;
        return value;
    }

    u16 CPU::fetch16()
    {
        u16 value = bus.read(regs.pc) | (bus.read(regs.pc + 1) << 8);
        regs.pc += 2;
        return value;
    }

    u8 CPU::read_r8(int index)
    {
        switch (index)
        {
        case 0:
            return regs.b;
        case 1:
            return regs.c;
        case 2:
            return regs.d;
        case 3:
            return regs.e;
        case 4:
            return regs.h;
        case 5:
            return regs.l;
        case 6:
            return bus.read(regs.hl);
        default:
            return regs.a; // 7
        }
    }

    void CPU::write_r8(int index, u8 value)
    {
        switch (index)
        {
        case 0:
            regs.b = value;
            break;
        case 1:
            regs.c = value;
            break;
        case 2:
            regs.d = value;
            break;
        case 3:
            regs.e = value;
            break;
        case 4:
            regs.h = value;
            break;
        case 5:
            regs.l = value;
            break;
        case 6:
            bus.write(regs.hl, value);
            break;
        default:
            regs.a = value;
            break; // 7
        }
    }

    void CPU::push16(u16 value)
    {
        regs.sp--;
        bus.write(regs.sp, static_cast<u8>(value >> 8));
        regs.sp--;
        bus.write(regs.sp, static_cast<u8>(value & 0xFF));
    }

    u16 CPU::pop16()
    {
        u8 low = bus.read(regs.sp);
        regs.sp++;
        u8 high = bus.read(regs.sp);
        regs.sp++;
        return static_cast<u16>((static_cast<u16>(high) << 8) | low);
    }

    void CPU::alu_op(int op, u8 value)
    {
        u8 a = regs.a;
        switch (op)
        {
        case 0: // ADD
        {
            int result = a + value;
            regs.set_flag(Registers::FLAG_Z, static_cast<u8>(result) == 0);
            regs.set_flag(Registers::FLAG_N, false);
            regs.set_flag(Registers::FLAG_H, ((a & 0x0F) + (value & 0x0F)) > 0x0F);
            regs.set_flag(Registers::FLAG_C, result > 0xFF);
            regs.a = static_cast<u8>(result);
            break;
        }
        case 1: // ADC
        {
            int carry = regs.flag(Registers::FLAG_C) ? 1 : 0;
            int result = a + value + carry;
            regs.set_flag(Registers::FLAG_Z, static_cast<u8>(result) == 0);
            regs.set_flag(Registers::FLAG_N, false);
            regs.set_flag(Registers::FLAG_H, ((a & 0x0F) + (value & 0x0F) + carry) > 0x0F);
            regs.set_flag(Registers::FLAG_C, result > 0xFF);
            regs.a = static_cast<u8>(result);
            break;
        }
        case 2: // SUB
        {
            int result = a - value;
            regs.set_flag(Registers::FLAG_Z, static_cast<u8>(result) == 0);
            regs.set_flag(Registers::FLAG_N, true);
            regs.set_flag(Registers::FLAG_H, (a & 0x0F) < (value & 0x0F));
            regs.set_flag(Registers::FLAG_C, a < value);
            regs.a = static_cast<u8>(result);
            break;
        }
        case 3: // SBC
        {
            int carry = regs.flag(Registers::FLAG_C) ? 1 : 0;
            int result = a - value - carry;
            regs.set_flag(Registers::FLAG_Z, static_cast<u8>(result) == 0);
            regs.set_flag(Registers::FLAG_N, true);
            regs.set_flag(Registers::FLAG_H, ((a & 0x0F) - (value & 0x0F) - carry) < 0);
            regs.set_flag(Registers::FLAG_C, result < 0);
            regs.a = static_cast<u8>(result);
            break;
        }
        case 4: // AND
            regs.a = static_cast<u8>(a & value);
            regs.set_flag(Registers::FLAG_Z, regs.a == 0);
            regs.set_flag(Registers::FLAG_N, false);
            regs.set_flag(Registers::FLAG_H, true);
            regs.set_flag(Registers::FLAG_C, false);
            break;
        case 5: // XOR
            regs.a = static_cast<u8>(a ^ value);
            regs.set_flag(Registers::FLAG_Z, regs.a == 0);
            regs.set_flag(Registers::FLAG_N, false);
            regs.set_flag(Registers::FLAG_H, false);
            regs.set_flag(Registers::FLAG_C, false);
            break;
        case 6: // OR
            regs.a = static_cast<u8>(a | value);
            regs.set_flag(Registers::FLAG_Z, regs.a == 0);
            regs.set_flag(Registers::FLAG_N, false);
            regs.set_flag(Registers::FLAG_H, false);
            regs.set_flag(Registers::FLAG_C, false);
            break;
        default: // 7: CP -- like SUB but result isn't stored
        {
            int result = a - value;
            regs.set_flag(Registers::FLAG_Z, static_cast<u8>(result) == 0);
            regs.set_flag(Registers::FLAG_N, true);
            regs.set_flag(Registers::FLAG_H, (a & 0x0F) < (value & 0x0F));
            regs.set_flag(Registers::FLAG_C, a < value);
            break;
        }
        }
    }

    u8 CPU::shift_op(int op, u8 value)
    {
        u8 result = value;
        bool carry = false;

        switch (op)
        {
        case 0: // RLC
            carry = bit_test(value, 7);
            result = static_cast<u8>((value << 1) | (carry ? 1 : 0));
            break;
        case 1: // RRC
            carry = bit_test(value, 0);
            result = static_cast<u8>((value >> 1) | (carry ? 0x80 : 0));
            break;
        case 2: // RL
        {
            bool old_carry = regs.flag(Registers::FLAG_C);
            carry = bit_test(value, 7);
            result = static_cast<u8>((value << 1) | (old_carry ? 1 : 0));
            break;
        }
        case 3: // RR
        {
            bool old_carry = regs.flag(Registers::FLAG_C);
            carry = bit_test(value, 0);
            result = static_cast<u8>((value >> 1) | (old_carry ? 0x80 : 0));
            break;
        }
        case 4: // SLA
            carry = bit_test(value, 7);
            result = static_cast<u8>(value << 1);
            break;
        case 5: // SRA -- arithmetic shift: bit 7 is preserved, not zeroed
            carry = bit_test(value, 0);
            result = static_cast<u8>((value >> 1) | (value & 0x80));
            break;
        case 6: // SWAP -- swap the two nibbles, carry always cleared
            carry = false;
            result = static_cast<u8>((value << 4) | (value >> 4));
            break;
        default: // 7: SRL
            carry = bit_test(value, 0);
            result = static_cast<u8>(value >> 1);
            break;
        }

        regs.set_flag(Registers::FLAG_Z, result == 0);
        regs.set_flag(Registers::FLAG_N, false);
        regs.set_flag(Registers::FLAG_H, false);
        regs.set_flag(Registers::FLAG_C, carry);
        return result;
    }
}
