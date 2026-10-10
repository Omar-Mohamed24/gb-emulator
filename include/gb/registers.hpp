#pragma once
#include "types.hpp"

namespace gb
{

    class Registers
    {
    public:
        union
        {
            u16 af = 0;
            struct
            {
                u8 f;
                u8 a;
            };
        };

        union
        {
            u16 bc = 0;
            struct
            {
                u8 c;
                u8 b;
            };
        };

        union
        {
            u16 de = 0;
            struct
            {
                u8 e;
                u8 d;
            };
        };

        union
        {
            u16 hl = 0;
            struct
            {
                u8 l;
                u8 h;
            };
        };

        u16 sp = 0, pc = 0;

        // --- Flag helpers (bits of F) ---
        static constexpr u8 FLAG_Z = 1 << 7;
        static constexpr u8 FLAG_N = 1 << 6;
        static constexpr u8 FLAG_H = 1 << 5;
        static constexpr u8 FLAG_C = 1 << 4;

        bool flag(u8 mask) const { return (f & mask) != 0; }

        void set_flag(u8 mask, bool on)
        {
            if (on)
                f |= mask;
            else
                f &= ~mask;
            f &= 0xF0; // bits 0-3 of F are always 0 on real hardware
        }
    };

}