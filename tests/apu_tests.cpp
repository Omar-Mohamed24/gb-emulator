#include "gb/apu.hpp"
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

    // Starts channel 2 playing a steady tone at full volume, routed to both sides.
    void start_channel2(APU &apu)
    {
        apu.write(0xFF16, 0x80); // duty 50%, length 64
        apu.write(0xFF17, 0xF0); // volume 15, DAC on
        apu.write(0xFF18, 0x00);
        apu.write(0xFF19, 0x87); // frequency high bits 7, trigger
    }

}

int main()
{
    // ---- Registers and status ----
    {
        APU apu;
        apu.reset();
        check(apu.read(0xFF26) == 0xF0, "after reset NR52 reads 0xF0 (power on, no channels playing)");
    }
    {
        APU apu;
        apu.reset();
        apu.write(0xFF10, 0x00);
        check(apu.read(0xFF10) == 0x80, "NR10 reads back with its unused bit set");
        apu.write(0xFF11, 0x00);
        check(apu.read(0xFF11) == 0x3F, "NR11 reads back with its write-only length bits set");
    }
    {
        APU apu;
        apu.reset();
        start_channel2(apu);
        check((apu.read(0xFF26) & 0x02) != 0, "triggering channel 2 sets its status bit");
    }
    {
        APU apu;
        apu.reset();
        apu.write(0xFF1A, 0x80); // wave DAC on
        apu.write(0xFF1E, 0x80); // trigger
        check((apu.read(0xFF26) & 0x04) != 0, "triggering the wave channel sets its status bit");
    }
    {
        APU apu;
        apu.reset();
        apu.write(0xFF21, 0xF0); // noise DAC on
        apu.write(0xFF23, 0x80);
        check((apu.read(0xFF26) & 0x08) != 0, "triggering the noise channel sets its status bit");
    }
    {
        APU apu;
        apu.reset();
        apu.write(0xFF30, 0x12);
        apu.write(0xFF3F, 0xAB);
        check(apu.read(0xFF30) == 0x12 && apu.read(0xFF3F) == 0xAB, "wave RAM stores and returns its bytes");
    }

    // ---- Power ----
    {
        APU apu;
        apu.reset();
        start_channel2(apu);
        apu.write(0xFF26, 0x00); // power off
        check(apu.read(0xFF26) == 0x70, "powering off clears the channel status bits");
        apu.write(0xFF12, 0xF0);
        check(apu.read(0xFF12) == 0x00, "registers ignore writes while the power is off");
        apu.write(0xFF26, 0x80); // power on
        check(apu.read(0xFF26) == 0xF0, "powering back on leaves the channels silent");
    }

    // ---- Length counter ----
    {
        APU apu;
        apu.reset();
        apu.write(0xFF16, 0x3F); // length 1
        apu.write(0xFF17, 0xF0);
        apu.write(0xFF19, 0xC0); // trigger with length enabled
        check((apu.read(0xFF26) & 0x02) != 0, "channel 2 plays right after its trigger");
        apu.tick(65536); // a full frame sequencer cycle has at least four length clocks
        check((apu.read(0xFF26) & 0x02) == 0, "length counter silences the channel when it expires");
    }

    // ---- Output ----
    {
        APU apu;
        apu.reset();
        apu.tick(70224); // one frame
        std::vector<int16_t> silent = apu.take_samples();
        check(silent.size() == 2 * 738, "one frame produces 738 stereo sample pairs at 44100 Hz");
        bool all_zero = true;
        for (int16_t s : silent)
        {
            all_zero = all_zero && s == 0;
        }
        check(all_zero, "no channel playing gives silence");
    }
    {
        APU apu;
        apu.reset();
        start_channel2(apu);
        apu.tick(20000);
        std::vector<int16_t> out = apu.take_samples();
        bool any_nonzero = false;
        for (int16_t s : out)
        {
            any_nonzero = any_nonzero || s != 0;
        }
        check(any_nonzero, "a playing square channel produces non-silent samples");
    }
    {
        // NR51 0x20 routes channel 2 to the left speaker only.
        APU apu;
        apu.reset();
        apu.write(0xFF25, 0x20);
        start_channel2(apu);
        apu.tick(20000);
        std::vector<int16_t> out = apu.take_samples();
        bool left_has_sound = false;
        bool right_silent = true;
        for (size_t i = 0; i + 1 < out.size(); i += 2)
        {
            left_has_sound = left_has_sound || out[i] != 0;
            right_silent = right_silent && out[i + 1] == 0;
        }
        check(left_has_sound && right_silent, "NR51 panning sends channel 2 to the left side only");
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
