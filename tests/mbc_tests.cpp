#include "gb/mbc/mbc1.hpp"
#include "gb/mbc/mbc3.hpp"
#include <cstdio>
#include <vector>

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

    constexpr int CLOCK = 4194304; // CPU cycles per second

    // Latches the clock with the 0x00 then 0x01 sequence.
    void latch(MBC3 &mbc)
    {
        mbc.write(0x6000, 0x00);
        mbc.write(0x6000, 0x01);
    }

    // Selects a clock register (8 = S, 9 = M, 10 = H, 11 = DL, 12 = DH) and enables RAM/clock access.
    void select_register(MBC3 &mbc, u8 reg)
    {
        mbc.write(0x0000, 0x0A);
        mbc.write(0x4000, reg);
    }

}

int main()
{
    std::vector<u8> rom(0x8000, 0);

    // ---- Clock ----
    {
        MBC3 mbc(rom, 0x8000);
        mbc.has_rtc = true;
        select_register(mbc, 0x08);
        mbc.tick(CLOCK * 2 + 10);
        latch(mbc);
        check(mbc.read(0xA000) == 2, "clock advances one second per 4194304 cycles");
    }
    {
        MBC3 mbc(rom, 0x8000);
        mbc.has_rtc = true;
        mbc.tick(CLOCK * 125);
        select_register(mbc, 0x09);
        latch(mbc);
        check(mbc.read(0xA000) == 2, "125 seconds reads as 2 minutes");
        select_register(mbc, 0x08);
        check(mbc.read(0xA000) == 5, "the seconds register is latched with the minutes (125 s leaves 5 s)");
    }
    {
        MBC3 mbc(rom, 0x8000);
        mbc.has_rtc = true;
        select_register(mbc, 0x0C);
        mbc.write(0xA000, 0x40); // halt bit
        mbc.tick(CLOCK * 10);
        select_register(mbc, 0x08);
        latch(mbc);
        check(mbc.read(0xA000) == 0, "a halted clock does not advance");
    }
    {
        // 511 days, 23:59:59 -> one more second wraps the day counter and sets the carry flag.
        MBC3 mbc(rom, 0x8000);
        mbc.has_rtc = true;
        select_register(mbc, 0x08);
        mbc.write(0xA000, 59);
        select_register(mbc, 0x09);
        mbc.write(0xA000, 59);
        select_register(mbc, 0x0A);
        mbc.write(0xA000, 23);
        select_register(mbc, 0x0B);
        mbc.write(0xA000, 0xFF);
        select_register(mbc, 0x0C);
        mbc.write(0xA000, 0x01);
        mbc.tick(CLOCK);
        latch(mbc);
        check(mbc.read(0xA000) == 0x80, "day counter overflow sets the carry flag and clears the day");
        select_register(mbc, 0x0B);
        latch(mbc);
        check(mbc.read(0xA000) == 0x00, "day counter restarts at 0 after overflow");
    }
    {
        // Clock registers are hidden when the cartridge has no RTC.
        MBC3 mbc(rom, 0x8000);
        select_register(mbc, 0x08);
        latch(mbc);
        check(mbc.read(0xA000) == 0xFF, "RTC registers read 0xFF on a cartridge without a clock");
    }

    // ---- Save data ----
    {
        // Elapsed time while the game is closed (90 s) is added when the save is loaded.
        MBC3 saved(rom, 0x2000);
        saved.has_rtc = true;
        saved.battery = true;
        std::vector<u8> data = saved.save_data(1000);

        MBC3 loaded(rom, 0x2000);
        loaded.has_rtc = true;
        loaded.battery = true;
        loaded.load_data(data, 1090);
        select_register(loaded, 0x08);
        latch(loaded);
        check(loaded.read(0xA000) == 30, "clock catches up on the 90 seconds that passed while closed");
        select_register(loaded, 0x09);
        latch(loaded);
        check(loaded.read(0xA000) == 1, "clock catches up into minutes");
    }
    {
        // A halted clock keeps its time across a save.
        MBC3 saved(rom, 0x2000);
        saved.has_rtc = true;
        select_register(saved, 0x0C);
        saved.write(0xA000, 0x40);
        std::vector<u8> data = saved.save_data(1000);

        MBC3 loaded(rom, 0x2000);
        loaded.has_rtc = true;
        loaded.load_data(data, 5000);
        select_register(loaded, 0x08);
        latch(loaded);
        check(loaded.read(0xA000) == 0, "a halted clock does not catch up on load");
    }
    {
        // Battery RAM round trip on an MBC1 cartridge.
        MBC1 first(rom, 0x2000);
        first.write(0x0000, 0x0A);
        first.write(0xA000, 0x42);
        std::vector<u8> data = first.save_data(0);

        MBC1 second(rom, 0x2000);
        second.load_data(data, 0);
        second.write(0x0000, 0x0A);
        check(second.read(0xA000) == 0x42, "battery RAM contents survive a save and load");
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
