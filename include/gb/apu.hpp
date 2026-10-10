#pragma once
#include <cstdint>
#include <vector>
#include "gb/types.hpp"

namespace gb
{
    // The sound unit: two square waves (channel 1 has a frequency sweep), a wave channel that plays
    // 4-bit samples from wave RAM, and a noise channel. Output is 16-bit stereo at SAMPLE_RATE.
    class APU
    {
    public:
        static constexpr int SAMPLE_RATE = 44100;

        void reset();
        void tick(int cycles);
        u8 read(u16 addr) const;
        void write(u16 addr, u8 value);
        std::vector<int16_t> take_samples();

    private:
        struct Envelope
        {
            int initial = 0;
            bool increase = false;
            int period = 0;
            int volume = 0;
            int timer = 0;
        };

        struct Square
        {
            bool on = false;      
            bool dac = false;     
            int duty = 0;         
            int duty_pos = 0;     
            int length = 0;       
            bool length_on = false;
            int freq = 0;         
            int timer = 0;    
            Envelope env;

            // Sweep (channel 1 only)
            int sweep_period = 0;
            bool sweep_negate = false;
            int sweep_shift = 0;
            int sweep_timer = 0;
            bool sweep_enabled = false;
            bool sweep_used_negate = false;
            int shadow_freq = 0;
        };

        struct Wave
        {
            bool on = false;
            bool dac = false;
            int length = 0; 
            bool length_on = false;
            int freq = 0;
            int timer = 0;
            int pos = 0;
            int volume_code = 0;
        };

        struct Noise
        {
            bool on = false;
            bool dac = false;
            int length = 0;
            bool length_on = false;
            Envelope env;
            int timer = 0;
            int divisor_code = 0;
            int shift = 0;
            bool width7 = false;
            u16 lfsr = 0x7FFF;
        };

        bool power = false;
        u8 regs[0x16] = {};
        u8 wave_ram[16] = {};
        Square ch1;
        Square ch2;
        Wave wave;
        Noise noise;

        int frame_counter = 0;
        int frame_step = 0;
        int sample_accum = 0;
        std::vector<int16_t> samples;

        void power_off();
        void trigger_square(Square &s);
        void trigger_sweep();
        int sweep_calc();
        void clock_frame_sequencer();
        void clock_length(bool &on, int &length, bool length_on);
        void clock_envelope(Envelope &env);
        void push_sample();
        float square_out(const Square &s) const;
        float wave_out() const;
        float noise_out() const;
    };
}
