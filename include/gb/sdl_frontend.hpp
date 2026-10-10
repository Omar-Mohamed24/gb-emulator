#pragma once
#include <cstdint>
#include <vector>
#include "gb/types.hpp"

struct SDL_Window;
struct SDL_Renderer;
struct SDL_Texture;

namespace gb
{
    class Joypad;
}

namespace gb
{
    class SdlFrontend
    {
    public:
        SdlFrontend();
        ~SdlFrontend();

        SdlFrontend(const SdlFrontend &) = delete;
        SdlFrontend &operator=(const SdlFrontend &) = delete;

        bool ok() const;
        void present(const std::vector<u8> &framebuffer);
        bool poll_events(Joypad &joypad);

        void queue_audio(const std::vector<int16_t> &samples);

    private:
        SDL_Window *window = nullptr;
        SDL_Renderer *renderer = nullptr;
        SDL_Texture *texture = nullptr;
        unsigned int audio_device = 0;
        bool sdl_initialized = false;
        std::vector<u32> pixels;
        unsigned int fps_window_start = 0;
        int fps_frames = 0;
        unsigned long long next_frame_time = 0;
    };
}
