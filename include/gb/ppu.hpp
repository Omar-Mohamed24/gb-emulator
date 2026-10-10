#pragma once
#include <vector>
#include "gb/types.hpp"

namespace gb
{

    class PPU
    {
    public:
        u8 lcdc = 0;         // LCD Control Register
        u8 stat = 0;         // LCD Status Register
        u8 scy = 0;          // Scroll Y
        u8 scx = 0;          // Scroll X
        u8 ly = 0;           // LCD Y Coordinate
        u8 lyc = 0;          // LY Compare
        u8 bgp = 0;          // Background Palette Data
        u16 line_cycles = 0; // Current line cycle count
        u8 mode = 0;         // Current PPU mode (0-3)
        u8 sp0 = 0;          // Sprite Palette 0 Data
        u8 sp1 = 0;          // Sprite Palette 1 Data
        u8 wy = 0;           // Window Y Position
        u8 wx = 0;           // Window X Position (screen column plus 7)
        u8 window_line = 0;  // Row inside the window; advances only on rows where the window is drawn

        std::vector<u8> framebuffer = std::vector<u8>(SCREEN_WIDTH * SCREEN_HEIGHT); // Framebuffer to store pixel data
        const std::vector<u8> *oam_source = nullptr;

        bool stat_line = false;
        bool stat_irq_requested = false;
        bool line_ready = false;

        bool tick(int cycles);
        u8 stat_read() const;
        int mode3_end() const;
        int next_boundary() const;
        std::vector<int> sprites_on_line(const std::vector<u8> &oam, int y) const;
        void render_line(const std::vector<u8> &vram, const std::vector<u8> &oam, int y);
        void render_background(const std::vector<u8> &vram, const std::vector<u8> &oam);
    };
}
