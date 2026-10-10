#include "gb/joypad.hpp"

namespace gb
{
    u8 Joypad::read() const
    {
        u8 low = 0x0F;
        if (!(select & 0x10))
        {
            low &= ~(held & 0x0F);
        }
        if (!(select & 0x20))
        {
            low &= ~((held >> 4) & 0x0F);
        }
        return 0xC0 | (select & 0x30) | low;
    }

    void Joypad::write(u8 value)
    {
        select = value & 0x30;
    }

    void Joypad::set_button(Button button, bool pressed)
    {
        u8 mask = static_cast<u8>(1 << button);
        bool was_pressed = (held & mask) != 0;

        if (pressed)
        {
            held |= mask;
        }
        else
        {
            held &= static_cast<u8>(~mask);
        }

        if (pressed && !was_pressed)
        {
            bool group_selected = (button < A) ? !(select & 0x10) : !(select & 0x20);
            if (group_selected)
            {
                interrupt_requested = true;
            }
        }
    }
}
