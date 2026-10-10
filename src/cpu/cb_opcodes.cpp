#include "gb/cpu.hpp"

namespace gb {

    // The whole CB table is regular: register index (0..7 = B,C,D,E,H,L,(HL),A)
    // is always the low 3 bits of the opcode.
    //   0x00-0x3F: RLC,RRC,RL,RR,SLA,SRA,SWAP,SRL r   (op index = opcode / 8)
    //   0x40-0x7F: BIT b,r
    //   0x80-0xBF: RES b,r
    //   0xC0-0xFF: SET b,r                            (bit index = (opcode/8) % 8)
    int CPU::execute_cb(u8 opcode)
    {
        int reg = opcode & 0x07;

        if (opcode <= 0x3F)
        {
            int op = opcode / 8;
            u8 value = read_r8(reg);
            u8 result = shift_op(op, value);
            write_r8(reg, result);
            return (reg == 6) ? 16 : 8;
        }

        int group = (opcode - 0x40) / 0x40; // 0 = BIT, 1 = RES, 2 = SET
        int bit = ((opcode - 0x40) / 8) % 8;
        u8 value = read_r8(reg);

        switch (group)
        {
        case 0: // BIT b,r -- reads only, never touches the carry flag
            regs.set_flag(Registers::FLAG_Z, !bit_test(value, bit));
            regs.set_flag(Registers::FLAG_N, false);
            regs.set_flag(Registers::FLAG_H, true);
            return (reg == 6) ? 12 : 8;

        case 1: // RES b,r -- no flags affected
            write_r8(reg, bit_clear(value, bit));
            return (reg == 6) ? 16 : 8;

        default: // SET b,r -- no flags affected
            write_r8(reg, bit_set(value, bit));
            return (reg == 6) ? 16 : 8;
        }
    }

}
