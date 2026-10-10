#include "gb/ppu.hpp"
#include <algorithm>

namespace gb
{
    int PPU::mode3_end() const
    {
        int sprites = 0;
        if (oam_source && ly < 144)
        {
            sprites = static_cast<int>(sprites_on_line(*oam_source, ly).size());
        }
        // Base pixel transfer, plus a penalty for the fine scroll, plus about 6 cycles per sprite on the row.
        return 80 + 172 + (scx & 0x07) + 6 * sprites;
    }

    int PPU::next_boundary() const
    {
        if (ly >= 144)
        {
            return 456;
        }
        if (line_cycles < 80)
        {
            return 80;
        }
        int end = mode3_end();
        if (line_cycles < end)
        {
            return end;
        }
        return 456;
    }

    bool PPU::tick(int cycles)
    {
        bool entered_vblank = false;
        int remaining = cycles;

        do
        {
            int step = std::min(remaining, next_boundary() - static_cast<int>(line_cycles));
            line_cycles = static_cast<u16>(line_cycles + step);
            remaining -= step;

            if (line_cycles >= 456)
            {
                line_cycles = static_cast<u16>(line_cycles - 456);
                ly++;

                if (ly == 144)
                {
                    entered_vblank = true;
                }

                if (ly > 153)
                {
                    ly = 0;
                    window_line = 0;
                }
            }

            u8 prev_mode = mode;
            if (ly < 144)
            {
                if (line_cycles < 80)
                {
                    mode = 2; // OAM Search
                }
                else if (line_cycles < mode3_end())
                {
                    mode = 3; // Pixel Transfer
                }
                else
                {
                    mode = 0; // HBlank
                }
            }
            else
            {
                mode = 1; // VBlank
            }

            if (ly < 144 && mode == 0 && prev_mode != 0)
            {
                line_ready = true;
            }

            bool stat_condition = ((stat & 0x40) && ly == lyc) || ((stat & 0x08) && mode == 0) || ((stat & 0x10) && mode == 1) || ((stat & 0x20) && mode == 2);
            if (stat_condition && !stat_line)
            {
                stat_irq_requested = true;
            }
            stat_line = stat_condition;
        } while (remaining > 0);

        return entered_vblank;
    }

    u8 PPU::stat_read() const
    {
        u8 value = 0x80 | (stat & 0x78) | (mode & 0x03);
        if (ly == lyc)
        {
            value |= 0x04;
        }
        return value;
    }

    std::vector<int> PPU::sprites_on_line(const std::vector<u8> &oam, int y) const
    {
        std::vector<int> found;
        const int height = (lcdc & 0x04) ? 16 : 8;
        for (int i = 0; i < 40 && found.size() < 10; i++)
        {
            int top = oam[i * 4] - 16;
            if (y >= top && y < top + height)
            {
                found.push_back(i);
            }
        }
        return found;
    }

    void PPU::render_line(const std::vector<u8> &vram, const std::vector<u8> &oam, int y)
    {
        u8 *row = &framebuffer[y * SCREEN_WIDTH];

        if (!(lcdc & 0x80))
        {
            std::fill(row, row + SCREEN_WIDTH, static_cast<u8>(0));
            return;
        }

        std::vector<u8> bg_color(SCREEN_WIDTH, 0);
        if (lcdc & 0x01)
        {
            const int map_base = (lcdc & 0x08) ? 0x1C00 : 0x1800; // 0x9C00 or 0x9800, relative to 0x8000
            const bool unsigned_tiles = (lcdc & 0x10) != 0;

            for (int x = 0; x < SCREEN_WIDTH; x++)
            {
                int px = (scx + x) & 0xFF;
                int py = (scy + y) & 0xFF;

                u8 tile_id = vram[map_base + (py / 8) * 32 + (px / 8)];

                int tile_offset;
                if (unsigned_tiles)
                {
                    tile_offset = tile_id * 16;
                }
                else
                {
                    tile_offset = 0x1000 + static_cast<int>(static_cast<i8>(tile_id)) * 16; // 0x9000 base
                }

                int row_in_tile = py % 8;
                int col = px % 8;
                u8 lo = vram[tile_offset + row_in_tile * 2];
                u8 hi = vram[tile_offset + row_in_tile * 2 + 1];

                int bit = 7 - col;
                u8 color = (((hi >> bit) & 1) << 1) | ((lo >> bit) & 1);
                bg_color[x] = color;
                row[x] = (bgp >> (color * 2)) & 0x03;
            }
        }
        else
        {
            std::fill(row, row + SCREEN_WIDTH, static_cast<u8>(0));
        }

        const int window_left = wx - 7;
        if ((lcdc & 0x20) && (lcdc & 0x01) && y >= wy && wx < 167)
        {
            const int win_map_base = (lcdc & 0x40) ? 0x1C00 : 0x1800;
            const bool unsigned_tiles = (lcdc & 0x10) != 0;

            for (int x = (window_left > 0 ? window_left : 0); x < SCREEN_WIDTH; x++)
            {
                int px = x - window_left;
                int py = window_line;

                u8 tile_id = vram[win_map_base + (py / 8) * 32 + (px / 8)];
                int tile_offset = unsigned_tiles
                                      ? tile_id * 16
                                      : 0x1000 + static_cast<int>(static_cast<i8>(tile_id)) * 16;

                int row_in_tile = py % 8;
                int col = px % 8;
                u8 lo = vram[tile_offset + row_in_tile * 2];
                u8 hi = vram[tile_offset + row_in_tile * 2 + 1];

                int bit = 7 - col;
                u8 color = (((hi >> bit) & 1) << 1) | ((lo >> bit) & 1);
                bg_color[x] = color;
                row[x] = (bgp >> (color * 2)) & 0x03;
            }
            window_line++;
        }

        if (!(lcdc & 0x02))
        {
            return;
        }

        const int height = (lcdc & 0x04) ? 16 : 8;
        const std::vector<int> sprites = sprites_on_line(oam, y);

        for (int x = 0; x < SCREEN_WIDTH; x++)
        {
            bool drawn = false;
            int best_left = 0;
            u8 best_shade = 0;

            for (int index : sprites)
            {
                int base = index * 4;
                int top = oam[base] - 16;
                int left = oam[base + 1] - 8;
                u8 tile = oam[base + 2];
                u8 attr = oam[base + 3];

                if (x < left || x >= left + 8)
                {
                    continue;
                }
                if (drawn && left >= best_left)
                {
                    continue; // an earlier sprite with a smaller left edge already wins here
                }

                int sy = y - top;
                if (attr & 0x40)
                {
                    sy = height - 1 - sy; // vertical flip
                }
                int tile_index = tile;
                if (height == 16)
                {
                    tile_index &= 0xFE;
                    if (sy >= 8)
                    {
                        tile_index++;
                        sy -= 8;
                    }
                }

                int sx = x - left;
                if (attr & 0x20)
                {
                    sx = 7 - sx; // horizontal flip
                }

                int offset = tile_index * 16 + sy * 2;
                u8 lo = vram[offset];
                u8 hi = vram[offset + 1];
                int bit = 7 - sx;
                u8 color = (((hi >> bit) & 1) << 1) | ((lo >> bit) & 1);

                if (color == 0)
                {
                    continue; // color 0 is transparent for sprites
                }
                if ((attr & 0x80) && bg_color[x] != 0)
                {
                    continue; // priority bit: sprite stays behind background colors 1-3
                }

                u8 palette = (attr & 0x10) ? sp1 : sp0;
                drawn = true;
                best_left = left;
                best_shade = (palette >> (color * 2)) & 0x03;
            }

            if (drawn)
            {
                row[x] = best_shade;
            }
        }
    }

    void PPU::render_background(const std::vector<u8> &vram, const std::vector<u8> &oam)
    {
        for (int y = 0; y < SCREEN_HEIGHT; y++)
        {
            render_line(vram, oam, y);
        }
    }
}
