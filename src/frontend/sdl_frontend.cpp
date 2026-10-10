#include "gb/sdl_frontend.hpp"
#include "gb/types.hpp"
#include "gb/joypad.hpp"
#include "gb/apu.hpp"
#include <SDL.h>
#include <cstdio>

namespace gb
{
    namespace
    {
        constexpr int WINDOW_SCALE = 4;
        constexpr u32 PALETTE[4] = {0xFFFFFFFF, 0xFFAAAAAA, 0xFF555555, 0xFF000000};
        constexpr Uint32 MAX_QUEUED_AUDIO_BYTES = APU::SAMPLE_RATE * 4 / 5;
    }

    SdlFrontend::SdlFrontend()
    {
        if (SDL_Init(SDL_INIT_VIDEO) != 0)
        {
            fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
            return;
        }
        sdl_initialized = true;

        if (SDL_InitSubSystem(SDL_INIT_AUDIO) != 0)
        {
            fprintf(stderr, "Audio unavailable: %s\n", SDL_GetError());
        }
        else
        {
            SDL_AudioSpec want = {};
            want.freq = APU::SAMPLE_RATE;
            want.format = AUDIO_S16SYS;
            want.channels = 2;
            want.samples = 1024;
            audio_device = SDL_OpenAudioDevice(nullptr, 0, &want, nullptr, 0);
            if (!audio_device)
            {
                fprintf(stderr, "SDL_OpenAudioDevice failed: %s\n", SDL_GetError());
            }
            else
            {
                SDL_PauseAudioDevice(audio_device, 0);
            }
        }

        window = SDL_CreateWindow("Game Boy",
                                  SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                  SCREEN_WIDTH * WINDOW_SCALE, SCREEN_HEIGHT * WINDOW_SCALE,
                                  0);
        if (!window)
        {
            fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
            return;
        }

        renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
        if (!renderer)
        {
            fprintf(stderr, "SDL_CreateRenderer failed: %s\n", SDL_GetError());
            return;
        }

        SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");
        texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING,
                                    SCREEN_WIDTH, SCREEN_HEIGHT);
        if (!texture)
        {
            fprintf(stderr, "SDL_CreateTexture failed: %s\n", SDL_GetError());
            return;
        }

        pixels.resize(SCREEN_WIDTH * SCREEN_HEIGHT);
    }

    SdlFrontend::~SdlFrontend()
    {
        if (audio_device)
        {
            SDL_CloseAudioDevice(audio_device);
        }
        if (texture)
        {
            SDL_DestroyTexture(texture);
        }
        if (renderer)
        {
            SDL_DestroyRenderer(renderer);
        }
        if (window)
        {
            SDL_DestroyWindow(window);
        }
        if (sdl_initialized)
        {
            SDL_Quit();
        }
    }

    bool SdlFrontend::ok() const
    {
        return window && renderer && texture;
    }

    void SdlFrontend::present(const std::vector<u8> &framebuffer)
    {
        for (int i = 0; i < SCREEN_WIDTH * SCREEN_HEIGHT; i++)
        {
            pixels[i] = PALETTE[framebuffer[i] & 0x03];
        }

        SDL_UpdateTexture(texture, nullptr, pixels.data(), SCREEN_WIDTH * sizeof(u32));
        SDL_RenderClear(renderer);
        SDL_RenderCopy(renderer, texture, nullptr, nullptr);
        SDL_RenderPresent(renderer);

        fps_frames++;
        unsigned int now = SDL_GetTicks();
        if (now - fps_window_start >= 1000)
        {
            char title[64];
            snprintf(title, sizeof(title), "Game Boy - %d fps", fps_frames * 1000 / static_cast<int>(now - fps_window_start));
            SDL_SetWindowTitle(window, title);
            fps_frames = 0;
            fps_window_start = now;
        }

        const unsigned long long freq = SDL_GetPerformanceFrequency();
        const unsigned long long frame_ticks = freq * 70224 / 4194304;
        unsigned long long current = SDL_GetPerformanceCounter();
        if (next_frame_time == 0)
        {
            next_frame_time = current;
        }
        next_frame_time += frame_ticks;
        if (next_frame_time > current)
        {
            const unsigned long long spin_margin = freq * 2 / 1000;
            if (next_frame_time > current + spin_margin)
            {
                SDL_Delay(static_cast<Uint32>((next_frame_time - current - spin_margin) * 1000 / freq));
            }
            while (SDL_GetPerformanceCounter() < next_frame_time)
            {
            }
        }
        else
        {
            next_frame_time = current;
        }
    }

    void SdlFrontend::queue_audio(const std::vector<int16_t> &samples)
    {
        if (!audio_device || samples.empty())
        {
            return;
        }
        if (SDL_GetQueuedAudioSize(audio_device) > MAX_QUEUED_AUDIO_BYTES)
        {
            return;
        }
        SDL_QueueAudio(audio_device, samples.data(), static_cast<Uint32>(samples.size() * sizeof(int16_t)));
    }

    bool SdlFrontend::poll_events(Joypad &joypad)
    {
        SDL_Event event;
        while (SDL_PollEvent(&event))
        {
            if (event.type == SDL_QUIT)
            {
                return false;
            }
            if ((event.type == SDL_KEYDOWN || event.type == SDL_KEYUP) && !event.key.repeat)
            {
                bool pressed = event.type == SDL_KEYDOWN;
                switch (event.key.keysym.sym)
                {
                case SDLK_RIGHT:
                    joypad.set_button(Joypad::RIGHT, pressed);
                    break;
                case SDLK_LEFT:
                    joypad.set_button(Joypad::LEFT, pressed);
                    break;
                case SDLK_UP:
                    joypad.set_button(Joypad::UP, pressed);
                    break;
                case SDLK_DOWN:
                    joypad.set_button(Joypad::DOWN, pressed);
                    break;
                case SDLK_z:
                    joypad.set_button(Joypad::A, pressed);
                    break;
                case SDLK_x:
                    joypad.set_button(Joypad::B, pressed);
                    break;
                case SDLK_RSHIFT:
                    joypad.set_button(Joypad::SELECT, pressed);
                    break;
                case SDLK_RETURN:
                    joypad.set_button(Joypad::START, pressed);
                    break;
                default:
                    break;
                }
            }
        }
        return true;
    }
}
