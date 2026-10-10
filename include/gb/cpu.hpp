#pragma once
#include "gb/types.hpp"
#include "gb/bus.hpp"
#include "gb/registers.hpp"

namespace gb {

class CPU {
private:
    Bus &bus;

    u8 read_r8(int index);
    void write_r8(int index, u8 value);
    void alu_op(int op, u8 value);
    u8 shift_op(int op, u8 value);
    void push16(u16 value);
    u16 pop16();

public:
    Registers regs;
    bool halted = false;
    bool ime = false;
    int ime_delay = 0;

    CPU(Bus &bus) : bus(bus) {}
    int step();
    int execute(u8 opcode);
    int execute_cb(u8 opcode);
    u8 fetch8();
    u16 fetch16();
};
}