#include "gb/apu.hpp"
#include <algorithm>

namespace gb
{
    namespace
    {
        constexpr u8 DUTY[4][8] = {
            {0, 0, 0, 0, 0, 0, 0, 1},
            {1, 0, 0, 0, 0, 0, 0, 1},
            {1, 0, 0, 0, 0, 1, 1, 1},
            {0, 1, 1, 1, 1, 1, 1, 0},
        };

        constexpr int NOISE_DIVISORS[8] = {8, 16, 32, 48, 64, 80, 96, 112};
        constexpr int WAVE_SHIFT[4] = {4, 0, 1, 2};
        constexpr u8 READ_MASK[0x16] = {
            0x80, 0x3F, 0x00, 0xFF, 0xBF, // NR10-NR14
            0xFF, 0x3F, 0x00, 0xFF, 0xBF, // 0xFF15 (unused), NR21-NR24
            0x7F, 0xFF, 0x9F, 0xFF, 0xBF, // NR30-NR34
            0xFF, 0xFF, 0x00, 0x00, 0xBF, // 0xFF1F (unused), NR41-NR44
            0x00, 0x00,                   // NR50, NR51
        };

        float dac_level(int digital)
        {
            return digital / 7.5f - 1.0f;
        }

        int16_t to_sample(float value)
        {
            return static_cast<int16_t>(std::clamp(value, -1.0f, 1.0f) * 16000.0f);
        }
    }

    void APU::reset()
    {
        ch1 = Square{};
        ch2 = Square{};
        wave = Wave{};
        noise = Noise{};
        std::fill(regs, regs + sizeof(regs), static_cast<u8>(0));
        frame_counter = 0;
        frame_step = 0;
        sample_accum = 0;
        samples.clear();

        power = true;
        write(0xFF24, 0x77); // NR50: maximum master volume
        write(0xFF25, 0xF3); // NR51: channels routed to both sides
        write(0xFF12, 0xF3); // NR12 (channel 1 DAC on, no sound playing)
    }

    void APU::power_off()
    {
        power = false;
        std::fill(regs, regs + sizeof(regs), static_cast<u8>(0));
        ch1 = Square{};
        ch2 = Square{};
        wave = Wave{};
        noise = Noise{};
        frame_counter = 0;
        frame_step = 0;
    }

    u8 APU::read(u16 addr) const
    {
        if (addr >= 0xFF30 && addr <= 0xFF3F)
        {
            return wave_ram[addr - 0xFF30];
        }
        if (addr == 0xFF26)
        {
            u8 status = (power ? 0x80 : 0x00) | 0x70;
            status |= ch1.on ? 0x01 : 0x00;
            status |= ch2.on ? 0x02 : 0x00;
            status |= wave.on ? 0x04 : 0x00;
            status |= noise.on ? 0x08 : 0x00;
            return status;
        }
        if (addr < 0xFF10 || addr > 0xFF25)
        {
            return 0xFF;
        }
        return static_cast<u8>(regs[addr - 0xFF10] | READ_MASK[addr - 0xFF10]);
    }

    void APU::write(u16 addr, u8 value)
    {
        if (addr >= 0xFF30 && addr <= 0xFF3F)
        {
            wave_ram[addr - 0xFF30] = value;
            return;
        }
        if (addr == 0xFF26)
        {
            bool on = (value & 0x80) != 0;
            if (on && !power)
            {
                power = true;
                frame_counter = 0;
                frame_step = 0;
            }
            else if (!on && power)
            {
                power_off();
            }
            return;
        }

        if (!power || addr < 0xFF10 || addr > 0xFF25)
        {
            return;
        }
        regs[addr - 0xFF10] = value;

        switch (addr)
        {
        case 0xFF10: // NR10: sweep
        {
            bool negate = (value & 0x08) != 0;
            if (ch1.sweep_negate && !negate && ch1.sweep_used_negate)
            {
                ch1.on = false; // clearing negate after a negative sweep silences channel 1
            }
            ch1.sweep_period = (value >> 4) & 0x07;
            ch1.sweep_negate = negate;
            ch1.sweep_shift = value & 0x07;
            break;
        }
        case 0xFF11: // NR11: duty and length
            ch1.duty = value >> 6;
            ch1.length = 64 - (value & 0x3F);
            break;
        case 0xFF12: // NR12: envelope; the top 5 bits turn the DAC on or off
            ch1.env.initial = value >> 4;
            ch1.env.increase = (value & 0x08) != 0;
            ch1.env.period = value & 0x07;
            ch1.dac = (value & 0xF8) != 0;
            if (!ch1.dac)
            {
                ch1.on = false;
            }
            break;
        case 0xFF13: // NR13: frequency low
            ch1.freq = (ch1.freq & 0x700) | value;
            break;
        case 0xFF14: // NR14: frequency high, length enable, trigger
            ch1.freq = (ch1.freq & 0x0FF) | ((value & 0x07) << 8);
            ch1.length_on = (value & 0x40) != 0;
            if (value & 0x80)
            {
                trigger_square(ch1);
                trigger_sweep();
            }
            break;

        case 0xFF16: // NR21
            ch2.duty = value >> 6;
            ch2.length = 64 - (value & 0x3F);
            break;
        case 0xFF17: // NR22
            ch2.env.initial = value >> 4;
            ch2.env.increase = (value & 0x08) != 0;
            ch2.env.period = value & 0x07;
            ch2.dac = (value & 0xF8) != 0;
            if (!ch2.dac)
            {
                ch2.on = false;
            }
            break;
        case 0xFF18: // NR23
            ch2.freq = (ch2.freq & 0x700) | value;
            break;
        case 0xFF19: // NR24
            ch2.freq = (ch2.freq & 0x0FF) | ((value & 0x07) << 8);
            ch2.length_on = (value & 0x40) != 0;
            if (value & 0x80)
            {
                trigger_square(ch2);
            }
            break;

        case 0xFF1A: // NR30: wave DAC
            wave.dac = (value & 0x80) != 0;
            if (!wave.dac)
            {
                wave.on = false;
            }
            break;
        case 0xFF1B: // NR31
            wave.length = 256 - value;
            break;
        case 0xFF1C: // NR32: output level
            wave.volume_code = (value >> 5) & 0x03;
            break;
        case 0xFF1D: // NR33
            wave.freq = (wave.freq & 0x700) | value;
            break;
        case 0xFF1E: // NR34
            wave.freq = (wave.freq & 0x0FF) | ((value & 0x07) << 8);
            wave.length_on = (value & 0x40) != 0;
            if (value & 0x80)
            {
                wave.on = wave.dac;
                if (wave.length == 0)
                {
                    wave.length = 256;
                }
                wave.timer = (2048 - wave.freq) * 2;
                wave.pos = 0;
            }
            break;

        case 0xFF20: // NR41
            noise.length = 64 - (value & 0x3F);
            break;
        case 0xFF21: // NR42
            noise.env.initial = value >> 4;
            noise.env.increase = (value & 0x08) != 0;
            noise.env.period = value & 0x07;
            noise.dac = (value & 0xF8) != 0;
            if (!noise.dac)
            {
                noise.on = false;
            }
            break;
        case 0xFF22: // NR43: LFSR clock
            noise.divisor_code = value & 0x07;
            noise.width7 = (value & 0x08) != 0;
            noise.shift = value >> 4;
            break;
        case 0xFF23: // NR44: length enable, trigger
            noise.length_on = (value & 0x40) != 0;
            if (value & 0x80)
            {
                noise.on = noise.dac;
                if (noise.length == 0)
                {
                    noise.length = 64;
                }
                noise.timer = NOISE_DIVISORS[noise.divisor_code] << noise.shift;
                noise.env.volume = noise.env.initial;
                noise.env.timer = noise.env.period ? noise.env.period : 8;
                noise.lfsr = 0x7FFF;
            }
            break;

        default:
            break;
        }
    }

    void APU::trigger_square(Square &s)
    {
        s.on = s.dac;
        if (s.length == 0)
        {
            s.length = 64;
        }
        s.timer = (2048 - s.freq) * 4;
        s.env.volume = s.env.initial;
        s.env.timer = s.env.period ? s.env.period : 8;
    }

    void APU::trigger_sweep()
    {
        ch1.shadow_freq = ch1.freq;
        ch1.sweep_timer = ch1.sweep_period ? ch1.sweep_period : 8;
        ch1.sweep_enabled = ch1.sweep_period != 0 || ch1.sweep_shift != 0;
        ch1.sweep_used_negate = false;
        if (ch1.sweep_shift != 0)
        {
            sweep_calc();
        }
    }

    int APU::sweep_calc()
    {
        int delta = ch1.shadow_freq >> ch1.sweep_shift;
        int next = ch1.sweep_negate ? ch1.shadow_freq - delta : ch1.shadow_freq + delta;
        if (ch1.sweep_negate)
        {
            ch1.sweep_used_negate = true;
        }
        if (next > 2047)
        {
            ch1.on = false;
        }
        return next;
    }

    void APU::clock_length(bool &on, int &length, bool length_on)
    {
        if (length_on && length > 0 && --length == 0)
        {
            on = false;
        }
    }

    void APU::clock_envelope(Envelope &env)
    {
        if (env.period == 0)
        {
            return;
        }
        if (--env.timer <= 0)
        {
            env.timer = env.period;
            if (env.increase && env.volume < 15)
            {
                env.volume++;
            }
            else if (!env.increase && env.volume > 0)
            {
                env.volume--;
            }
        }
    }

    void APU::clock_frame_sequencer()
    {
        frame_step = (frame_step + 1) & 7;

        if ((frame_step & 1) == 0)
        {
            clock_length(ch1.on, ch1.length, ch1.length_on);
            clock_length(ch2.on, ch2.length, ch2.length_on);
            clock_length(wave.on, wave.length, wave.length_on);
            clock_length(noise.on, noise.length, noise.length_on);
        }

        if (frame_step == 2 || frame_step == 6)
        {
            if (--ch1.sweep_timer <= 0)
            {
                ch1.sweep_timer = ch1.sweep_period ? ch1.sweep_period : 8;
                if (ch1.sweep_enabled && ch1.sweep_period != 0)
                {
                    int next = sweep_calc();
                    if (next <= 2047 && ch1.sweep_shift != 0)
                    {
                        ch1.freq = next;
                        ch1.shadow_freq = next;
                        sweep_calc();
                    }
                }
            }
        }

        if (frame_step == 7)
        {
            clock_envelope(ch1.env);
            clock_envelope(ch2.env);
            clock_envelope(noise.env);
        }
    }

    float APU::square_out(const Square &s) const
    {
        if (!s.on || !s.dac)
        {
            return 0.0f;
        }
        int digital = DUTY[s.duty][s.duty_pos] ? s.env.volume : 0;
        return dac_level(digital);
    }

    float APU::wave_out() const
    {
        if (!wave.on || !wave.dac)
        {
            return 0.0f;
        }
        u8 byte = wave_ram[wave.pos / 2];
        int nibble = (wave.pos & 1) ? (byte & 0x0F) : (byte >> 4);
        return dac_level(nibble >> WAVE_SHIFT[wave.volume_code]);
    }

    float APU::noise_out() const
    {
        if (!noise.on || !noise.dac)
        {
            return 0.0f;
        }
        int digital = (noise.lfsr & 1) ? 0 : noise.env.volume;
        return dac_level(digital);
    }

    void APU::push_sample()
    {
        const float outs[4] = {square_out(ch1), square_out(ch2), wave_out(), noise_out()};
        const u8 nr50 = regs[0x14];
        const u8 nr51 = regs[0x15];

        float left = 0.0f;
        float right = 0.0f;
        for (int i = 0; i < 4; i++)
        {
            if (nr51 & (0x10 << i))
            {
                left += outs[i];
            }
            if (nr51 & (0x01 << i))
            {
                right += outs[i];
            }
        }

        float left_volume = (((nr50 >> 4) & 0x07) + 1) / 8.0f;
        float right_volume = ((nr50 & 0x07) + 1) / 8.0f;
        samples.push_back(to_sample(left / 4.0f * left_volume));
        samples.push_back(to_sample(right / 4.0f * right_volume));
    }

    std::vector<int16_t> APU::take_samples()
    {
        std::vector<int16_t> out;
        out.swap(samples);
        return out;
    }

    void APU::tick(int cycles)
    {
        for (int i = 0; i < cycles; i++)
        {
            if (power)
            {
                if (--ch1.timer <= 0)
                {
                    ch1.timer = (2048 - ch1.freq) * 4;
                    ch1.duty_pos = (ch1.duty_pos + 1) & 7;
                }
                if (--ch2.timer <= 0)
                {
                    ch2.timer = (2048 - ch2.freq) * 4;
                    ch2.duty_pos = (ch2.duty_pos + 1) & 7;
                }
                if (--wave.timer <= 0)
                {
                    wave.timer = (2048 - wave.freq) * 2;
                    wave.pos = (wave.pos + 1) & 31;
                }
                if (--noise.timer <= 0)
                {
                    noise.timer = NOISE_DIVISORS[noise.divisor_code] << noise.shift;
                    u16 bit = static_cast<u16>((noise.lfsr ^ (noise.lfsr >> 1)) & 1);
                    noise.lfsr = static_cast<u16>((noise.lfsr >> 1) | (bit << 14));
                    if (noise.width7)
                    {
                        noise.lfsr = static_cast<u16>((noise.lfsr & ~0x40) | (bit << 6));
                    }
                }

                if (++frame_counter == 8192)
                {
                    frame_counter = 0;
                    clock_frame_sequencer();
                }
            }

            sample_accum += SAMPLE_RATE;
            if (sample_accum >= CPU_CLOCK_HZ)
            {
                sample_accum -= CPU_CLOCK_HZ;
                push_sample();
            }
        }
    }
}
