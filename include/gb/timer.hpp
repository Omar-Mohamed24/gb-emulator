#pragma once
#include "gb/types.hpp"

namespace gb {
class Timer {
    public:
        u16 div = 0; // Divider register (0xFF04)
        u16 tima = 0; // Timer counter (0xFF05)
        u16 tma = 0; // Timer modulo (0xFF06)
        u16 tac = 0; // Timer control (0xFF07)

        int timer_counter = 0;

        bool tick(int cycles);
    };
}