#pragma once
#include <cstdint>

namespace gb {

using u8  = std::uint8_t;
using u16 = std::uint16_t;
using u32 = std::uint32_t;
using u64 = std::uint64_t;
using i8  = std::int8_t;

constexpr int SCREEN_WIDTH  = 160;
constexpr int SCREEN_HEIGHT = 144;
constexpr int CPU_CLOCK_HZ  = 4194304;
constexpr int CYCLES_PER_FRAME = 70224;

constexpr bool bit_test(u8 value, int bit) { return (value >> bit) & 1; }
constexpr u8   bit_set(u8 value, int bit)  { return value | (1 << bit); }
constexpr u8   bit_clear(u8 value, int bit){ return value & ~(1 << bit); }

}