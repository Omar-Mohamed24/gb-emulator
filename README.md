# gb-emulator

A Game Boy (DMG) emulator written in C++20. It emulates the original handheld's CPU, graphics, timers, input, sound, and common cartridge types, and runs in a window using SDL2.

## Features

| Area | Status |
| --- | --- |
| CPU | Sharp LR35902: all legal base opcodes and the CB-prefixed table. Undefined opcodes stop the emulator with a message. |
| Interrupts | VBlank, LCD STAT, timer, and joypad. `EI` delay and `HALT` wake-up are implemented. |
| Timer | `DIV`, `TIMA`, `TMA`, and `TAC` with all four rates. |
| Graphics | Background, window, and sprites (8×8 and 8×16) with flips, priority, and the 10-sprite-per-line limit. Mode 3 length varies with sprites and fine scroll. STAT is evaluated at each mode boundary. |
| OAM DMA | Copies 160 bytes on a write to `0xFF46` (performed instantly, not cycle by cycle). |
| Input | Joypad register with button groups and an interrupt on press. |
| Sound | Two square channels (channel 1 with sweep), a wave channel, and a noise channel. Envelopes, length counters, and stereo panning. |
| Cartridges | ROM only, MBC1, MBC3 (including the real-time clock), and MBC5 (including rumble bank masking). |
| Saves | Battery-backed RAM and the MBC3 clock are saved to a `.sav` file next to the ROM. |

Not supported: Game Boy Color features (GBC-only games such as *Dango Dash* stop on a DMG), MBC2, MBC6, MBC7, MBC1M, HuC1/HuC3, MMM01, the Game Boy Camera, and the link cable. The serial port only captures bytes written to `SB`; no link cable is emulated.

## Building

You need a C++20 compiler, CMake 3.16 or newer, and SDL2 for the window. The emulator core and its tests build without SDL2.

### Windows (Visual Studio)

```powershell
cmake -B build
cmake --build build --config Release
.\build\Release\gb-emulator.exe roms\dmg-acid2.gb
```

If CMake cannot find SDL2, point `SDL2_DIR` at the folder that contains `SDL2Config.cmake` and reconfigure. The SDL2 DLL is copied next to the executable automatically.

### Linux and macOS

```sh
sudo apt install libsdl2-dev   # or: brew install sdl2
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
./build/gb-emulator roms/dmg-acid2.gb
```

Build in `Release` for normal play. The `Debug` default is much slower, and frame pacing can fall behind.

Without SDL2, CMake builds only the core and tests. The `gb-emulator` executable then runs headless for 60 frames and prints the screen as ASCII.

## Usage

```
gb-emulator <rom.gb>
```

The window is four times the Game Boy's 160×144 resolution and shows the frame rate in the title bar. Close the window to quit.

### Controls

| Game Boy | Keyboard |
| --- | --- |
| D-pad | Arrow keys |
| A | Z |
| B | X |
| Select | Right Shift |
| Start | Enter |

### Save files

For a cartridge with a battery, the RAM is written to `<rom name>.sav` when the window closes and loaded the next time the same ROM starts. For cartridges with a clock, the clock keeps running while the emulator is closed and catches up when the save is loaded.

The clock is stored in a format specific to this emulator. Save files from other emulators (such as BGB or VBA) may load their RAM correctly, but their clock data will not be read correctly.

Save files are written only on exit. If the emulator crashes, progress since the last start is lost.

## Testing

The unit tests are separate executables, and CTest runs them all:

```sh
cmake --build build --config Release
cd build && ctest -C Release --output-on-failure
```

| Suite | Covers |
| --- | --- |
| `cpu_tests` | Registers, memory map, instructions, interrupts, timer, joypad, MBC banking, DMA |
| `ppu_tests` | Line and mode timing, VBlank, background, sprites, window, mode 3 length |
| `mbc_tests` | MBC3 clock, latching, halt, day overflow, save and load |
| `apu_tests` | Register masks, channel status, length counters, power, panning, sample rate |

Test ROMs live in `roms/tests/`:

- Blargg's `cpu_instrs` (all 11 sub-tests, `01`–`11`) passes. The ROM reports its results over the serial port, which this emulator captures in `Bus::serial_output`. The main program doesn't print them yet, so the result has to be read from that buffer.
- `dmg-acid2` (by Matt Currie) checks the graphics pipeline. This emulator renders it correctly.

## Project layout

```
include/gb/        Public headers
  cpu.hpp          CPU and instruction execution
  bus.hpp          Memory map, I/O registers, cartridge loading and saves
  ppu.hpp          Picture processing unit (graphics)
  apu.hpp          Audio processing unit (sound)
  timer.hpp        DIV and TIMA timer
  joypad.hpp       Button input
  gameboy.hpp      Ties the components together and runs one step at a time
  mbc/             Cartridge bank controllers (ROM only, MBC1, MBC3, MBC5)
src/               Implementation of the headers above
  cpu/             Opcode tables (base and CB-prefixed)
  mbc/             Bank controller implementations
  frontend/        SDL2 window, audio, and input
  main.cpp         Command-line entry point
tests/             Unit test executables
roms/tests/        Test ROMs used during development
```

The emulator core (`gbcore`) has no SDL2 dependency. The SDL2 frontend is compiled only if SDL2 is found.

## Timing

The emulator runs at the hardware's speed of 4,194,304 cycles per second, which is about 59.73 frames per second. A software frame limiter keeps it at that rate. Audio is produced at 44,100 Hz and sent to the sound device as the frames are presented.

## Known limitations

- Mode 3 length uses an approximation of about six cycles per sprite. Real hardware varies by sprite position and background.
- The frame sequencer runs on its own counter rather than being tied to the `DIV` register, so games that rely on exact `DIV` writes may sound slightly different.
- Audio has no high-pass filter, so output may have a small DC offset compared with hardware.
- The clock format used in save files is specific to this emulator (see Save files).

## References

- [Pan Docs](https://gbdev.io/pandocs/): the main hardware reference
- [Opcode table](https://gbdev.io/gb-opcodes/optables/)
- Blargg's test ROMs and `dmg-acid2` are the work of their authors and are included for testing only.