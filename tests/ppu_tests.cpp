#include "gb/ppu.hpp"
#include "gb/registers.hpp"
#include <cstdio>

using namespace gb;

namespace
{

    int failures = 0;

    void check(bool condition, const char *description)
    {
        if (condition)
        {
            printf("[PASS] %s\n", description);
        }
        else
        {
            printf("[FAIL] %s\n", description);
            failures++;
        }
    }

}

int main()
{
    // ---- Line and mode timing ----
    {
        PPU ppu;
        ppu.tick(455);
        check(ppu.ly == 0, "455 cycles is still scanline 0");
        check(ppu.line_cycles == 455, "455 cycles leaves line_cycles at 455");
    }
    {
        PPU ppu;
        ppu.tick(456);
        check(ppu.ly == 1, "456 cycles advances to scanline 1");
        check(ppu.line_cycles == 0, "456 cycles resets line_cycles to 0");
        check(ppu.mode == 2, "start of a new line is OAM scan (mode 2)");
    }
    {
        PPU ppu;
        ppu.tick(79);
        check(ppu.mode == 2, "cycle 79 is still mode 2 (OAM scan)");
        ppu.tick(1);
        check(ppu.mode == 3, "cycle 80 enters mode 3 (pixel transfer)");
    }
    {
        PPU ppu;
        ppu.tick(251);
        check(ppu.mode == 3, "cycle 251 is still mode 3");
        ppu.tick(1);
        check(ppu.mode == 0, "cycle 252 enters mode 0 (HBlank)");
    }

    // ---- VBlank ----
    {
        PPU ppu;
        bool any_true = false;
        for (int i = 0; i < 143; i++)
        {
            any_true = ppu.tick(456) || any_true;
        }
        check(ppu.ly == 143, "143 full lines leaves ly at 143");
        check(!any_true, "no VBlank signal before line 144");
    }
    {
        PPU ppu;
        for (int i = 0; i < 143; i++)
        {
            ppu.tick(456);
        }
        bool entered = ppu.tick(456);
        check(entered, "tick() reports VBlank when ly reaches 144");
        check(ppu.ly == 144, "ly is 144 on entering VBlank");
        check(ppu.mode == 1, "entering VBlank sets mode 1");
    }
    {
        PPU ppu;
        for (int i = 0; i < 144; i++)
        {
            ppu.tick(456);
        }
        bool any_true = false;
        for (int i = 0; i < 9; i++)
        {
            any_true = ppu.tick(456) || any_true;
        }
        check(!any_true, "VBlank signal is not repeated for the rest of VBlank");
        check(ppu.ly == 153, "ly reaches 153 at the end of VBlank");
        check(ppu.mode == 1, "all of lines 144-153 stay in mode 1");
    }

    // ---- Frame wrap and once-per-frame signal ----
    {
        PPU ppu;
        int vblank_signals = 0;
        for (int i = 0; i < 154; i++)
        {
            if (ppu.tick(456))
            {
                vblank_signals++;
            }
        }
        check(vblank_signals == 1, "exactly one VBlank signal per full frame");
        check(ppu.ly == 0, "ly wraps back to 0 after 154 lines");
        check(ppu.mode == 2, "a new frame starts in mode 2");
    }

    // ---- Large single calls ----
    {
        PPU ppu;
        ppu.tick(456 * 3);
        check(ppu.ly == 3, "one tick() spanning 3 lines advances ly by 3");
    }
    {
        PPU ppu;
        for (int i = 0; i < 143; i++)
        {
            ppu.tick(456);
        }
        bool entered = ppu.tick(456 * 2);
        check(entered, "one tick() that crosses into VBlank still reports it");
        check(ppu.ly == 145, "a multi-line tick() across VBlank lands on the right line");
    }

    // ---- Background renderer ----
    {
        // Worked example: pixel (x=10, y=20), SCX=5, SCY=3, unsigned tiles,
        // map at 0x9800, BGP=0xE4. Map entry 65 -> tile 7; row 7 lo=0x81, hi=0x42;
        // pixel column 7 -> color 1 -> shade 1.
        PPU ppu;
        std::vector<u8> vram(0x2000, 0);
        std::vector<u8> oam(160, 0);
        ppu.lcdc = 0x91;
        ppu.scx = 5;
        ppu.scy = 3;
        ppu.bgp = 0xE4;
        vram[0x1800 + 65] = 7;
        vram[0x0070 + 7 * 2] = 0x81;
        vram[0x0070 + 7 * 2 + 1] = 0x42;
        ppu.render_background(vram, oam);
        check(ppu.framebuffer[20 * 160 + 10] == 1, "background pixel matches the hand-worked example (shade 1)");
    }
    {
        // Signed tiles: tile id 0x90 is -112, so its data starts at 0x8900.
        // Pixel (0,0) lo bit 7 = 1, hi bit 7 = 0 -> color 1 -> shade 1.
        PPU ppu;
        std::vector<u8> vram(0x2000, 0);
        std::vector<u8> oam(160, 0);
        ppu.lcdc = 0x81;
        ppu.bgp = 0xE4;
        vram[0x1800] = 0x90;
        vram[0x0900] = 0x80;
        vram[0x0901] = 0x00;
        ppu.render_background(vram, oam);
        check(ppu.framebuffer[0] == 1, "signed tile id 0x90 is read from 0x8900");
    }
    {
        PPU ppu;
        std::vector<u8> vram(0x2000, 0xFF);
        std::vector<u8> oam(160, 0);
        ppu.lcdc = 0x91;
        ppu.bgp = 0xE4;
        ppu.render_background(vram, oam);
        ppu.lcdc = 0x11; // LCD off
        ppu.render_background(vram, oam);
        bool all_zero = true;
        for (u8 v : ppu.framebuffer)
        {
            if (v != 0)
            {
                all_zero = false;
            }
        }
        check(all_zero, "LCD off clears the framebuffer");
    }

    // ---- Sprite selection by row ----
    {
        PPU ppu;
        std::vector<u8> oam(160, 0);
        ppu.lcdc = 0x80; // 8-row sprites
        oam[0] = 16;     // sprite 0 top edge on screen row 0
        check(ppu.sprites_on_line(oam, 0).size() == 1, "8-row sprite covers its first row");
        check(ppu.sprites_on_line(oam, 7).size() == 1, "8-row sprite covers its last row");
        check(ppu.sprites_on_line(oam, 8).empty(), "8-row sprite does not cover the row below it");
    }
    {
        PPU ppu;
        std::vector<u8> oam(160, 0);
        ppu.lcdc = 0x84; // 16-row sprites
        oam[0] = 16;
        check(ppu.sprites_on_line(oam, 8).size() == 1, "16-row sprite covers row 8");
        check(ppu.sprites_on_line(oam, 16).empty(), "16-row sprite stops after 16 rows");
    }
    {
        PPU ppu;
        std::vector<u8> oam(160, 0);
        ppu.lcdc = 0x80;
        for (int i = 0; i < 11; i++)
        {
            oam[i * 4] = 16; // eleven sprites, all covering row 0
        }
        std::vector<int> found = ppu.sprites_on_line(oam, 0);
        check(found.size() == 10, "at most 10 sprites are selected per row");
        check(found.back() == 9, "the first 10 sprites in table order are the ones kept");
    }

    // ---- Window ----
    {
        PPU ppu;
        std::vector<u8> vram(0x2000, 0);
        std::vector<u8> oam(160, 0);
        ppu.lcdc = 0xB1; // LCD on, window on, background on, window map 0x9800, unsigned tiles
        ppu.bgp = 0xE4;
        ppu.wy = 0;
        ppu.wx = 15; // window starts at screen column 8
        vram[0x1800] = 1;
        vram[16] = 0x80;
        vram[17] = 0x00;
        ppu.render_line(vram, oam, 0);
        check(ppu.framebuffer[8] == 1, "window starts at screen column WX - 7");
        check(ppu.framebuffer[7] == 0, "background is shown left of the window");
        check(ppu.window_line == 1, "window row counter advances after a window row");
    }
    {
        PPU ppu;
        std::vector<u8> vram(0x2000, 0);
        std::vector<u8> oam(160, 0);
        ppu.lcdc = 0x81; // window bit off
        ppu.bgp = 0xE4;
        ppu.wy = 0;
        ppu.wx = 15;
        vram[0x1800] = 1;
        vram[16] = 0x80;
        vram[17] = 0x00;
        ppu.render_line(vram, oam, 0);
        check(ppu.framebuffer[8] == 0, "window is not drawn when LCDC bit 5 is off");
        check(ppu.window_line == 0, "window row counter does not advance when the window is off");
    }

    // ---- Mode 3 length ----
    {
        PPU ppu;
        std::vector<u8> oam(160, 0);
        ppu.oam_source = &oam;
        ppu.lcdc = 0x80;
        check(ppu.mode3_end() == 252, "with no sprites and SCX 0, mode 3 ends at cycle 252");
    }
    {
        PPU ppu;
        std::vector<u8> oam(160, 0);
        ppu.oam_source = &oam;
        ppu.lcdc = 0x80;
        oam[0] = 16; // one sprite covering row 0
        ppu.scx = 0;
        check(ppu.mode3_end() == 258, "one sprite on the row adds 6 cycles (mode 3 ends at 258)");
        ppu.tick(257);
        check(ppu.mode == 3, "cycle 257 is still mode 3 with one sprite");
        ppu.tick(1);
        check(ppu.mode == 0, "cycle 258 enters mode 0 with one sprite");
    }
    {
        PPU ppu;
        std::vector<u8> oam(160, 0);
        ppu.oam_source = &oam;
        ppu.lcdc = 0x80;
        ppu.scx = 5;
        check(ppu.mode3_end() == 257, "SCX 5 adds a 5-cycle fine scroll penalty");
    }
    {
        PPU ppu;
        std::vector<u8> oam(160, 0);
        ppu.oam_source = &oam;
        ppu.lcdc = 0x80;
        ppu.scx = 0x0F; // only the low 3 bits (7) count
        check(ppu.mode3_end() == 259, "fine scroll uses only SCX & 7");
    }
    {
        PPU ppu;
        std::vector<u8> oam(160, 0);
        ppu.oam_source = &oam;
        ppu.lcdc = 0x80;
        for (int i = 0; i < 10; i++)
        {
            oam[i * 4] = 16; // ten sprites on row 0
        }
        check(ppu.mode3_end() == 312, "ten sprites on a row make mode 3 end at 312");
    }
    {
        // Sprites on a different row do not lengthen this row.
        PPU ppu;
        std::vector<u8> oam(160, 0);
        ppu.oam_source = &oam;
        ppu.lcdc = 0x80;
        oam[0] = 16 + 8; // starts on row 8
        check(ppu.mode3_end() == 252, "a sprite on another row does not change mode 3 length");
    }

    // ---- STAT is evaluated at each mode boundary ----
    {
        // HBlank STAT enabled. A single tick that spans the whole of mode 3
        // must still raise the interrupt, because the boundary is passed inside the tick.
        PPU ppu;
        ppu.stat = 0x08;
        ppu.tick(400);
        check(ppu.stat_irq_requested, "STAT HBlank interrupt is raised when a large tick crosses mode 0");
        check(ppu.mode == 0, "after crossing the boundary the mode is 0");
    }
    {
        // Sprite-extended mode 3 must move the HBlank STAT edge later.
        PPU ppu;
        std::vector<u8> oam(160, 0);
        ppu.oam_source = &oam;
        ppu.lcdc = 0x80;
        oam[0] = 16;
        ppu.stat = 0x08;
        ppu.tick(257);
        check(!ppu.stat_irq_requested, "no HBlank STAT interrupt before the sprite-extended mode 3 ends");
        ppu.tick(1);
        check(ppu.stat_irq_requested, "HBlank STAT interrupt fires at the sprite-extended boundary (258)");
    }
    {
        // A tick that ends exactly on a row boundary still lands on the correct new row.
        PPU ppu;
        ppu.tick(456 * 5 + 100);
        check(ppu.ly == 5 && ppu.line_cycles == 100, "a long tick lands on the correct row and cycle");
    }

    if (failures == 0)
    {
        printf("\nAll tests passed.\n");
    }
    else
    {
        printf("\n%d test(s) failed.\n", failures);
    }

    return failures == 0 ? 0 : 1;
}
