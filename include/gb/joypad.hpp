#pragma once
#include "gb/types.hpp"

namespace gb
{
    //   Button bits in `held` are 1 when pressed:
    //   bits 0-3: Right, Left, Up, Down (direction group)
    //   bits 4-7: A, B, Select, Start (action group)
    class Joypad
    {
    public:
        enum Button
        {
            RIGHT = 0,
            LEFT = 1,
            UP = 2,
            DOWN = 3,
            A = 4,
            B = 5,
            SELECT = 6,
            START = 7
        };

        u8 select = 0x30;
        u8 held = 0;
        bool interrupt_requested = false;

        u8 read() const;
        void write(u8 value);
        void set_button(Button button, bool pressed);
    };
}
