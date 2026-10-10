#include "gb/gameboy.hpp"
#include <cstdio>
#include <filesystem>
#include <iostream>
#ifdef GB_HAS_SDL
#include "gb/sdl_frontend.hpp"
#endif
using namespace gb;
using namespace std;

int main(int argc, char *argv[])
{
    if (argc < 2)
    {
        cerr << "Usage: " << argv[0] << " <rom.gb>\n";
        return 1;
    }

    GameBoy gb;
    bool success = gb.load_rom(argv[1]);
    if (!success)
    {
        cerr << "Failed to load ROM: " << argv[1] << "\n";
        return 1;
    }
    cout << "ROM loaded successfully: " << argv[1] << "\n";

    string save_path = filesystem::path(argv[1]).replace_extension(".sav").string();
    gb.bus.load_save(save_path);

    gb.reset();

#ifdef GB_HAS_SDL
    SdlFrontend frontend;
    if (!frontend.ok())
    {
        return 1;
    }

    bool running = true;
    while (running)
    {
        int cycles = 0;
        while (cycles < CYCLES_PER_FRAME)
        {
            cycles += gb.step();
        }

        frontend.present(gb.ppu.framebuffer);
        frontend.queue_audio(gb.bus.apu.take_samples());
        running = frontend.poll_events(gb.bus.joypad);
    }
#else
    for (int frame = 0; frame < 60; frame++)
    {
        int cycles = 0;
        while (cycles < CYCLES_PER_FRAME)
        {
            cycles += gb.step();
        }
    }
    printf("lcdc=%02X bgp=%02X scx=%d scy=%d\n", gb.ppu.lcdc, gb.ppu.bgp, gb.ppu.scx, gb.ppu.scy);
    const char shades[] = " .:#";
    for (int y = 0; y < SCREEN_HEIGHT; y++)
    {
        for (int x = 0; x < SCREEN_WIDTH; x++)
            putchar(shades[gb.ppu.framebuffer[y * SCREEN_WIDTH + x]]);
        putchar('\n');
    }
#endif

    if (!gb.bus.save_ram(save_path))
    {
        cerr << "Failed to write save file: " << save_path << "\n";
        return 1;
    }
    return 0;
}
