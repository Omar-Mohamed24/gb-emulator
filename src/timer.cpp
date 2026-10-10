#include "gb/timer.hpp"

namespace gb {
    bool Timer::tick(int cycles) {
        div += cycles;

        if(tac & 0x04) {
            int cnt = (tac & 0x03) == 0 ? 1024 : (tac & 0x03) == 1 ? 16 : (tac & 0x03) == 2 ? 64 : 256;
            timer_counter += cycles;

            while(timer_counter >= cnt) {
                timer_counter -= cnt;
                if(tima == 0xff) {
                    tima = tma;
                    return true; 
                }
                tima++;
            }
        }

        return false;
    }
}