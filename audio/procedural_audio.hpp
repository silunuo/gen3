#pragma once

// 三类角色各有音色；由动作事件触发，不持续播放背景音。

#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <windows.h>
#include <mmsystem.h>

#include <array>
#include <vector>

namespace pets {

class Audio {
public:
    bool enabled = false;

    Audio();
    ~Audio();

    void play(int type, double frequency, double volume = 0.15);

private:
    static constexpr int sampleRate = 22050;

    struct Slot {
        WAVEHDR header{};
        std::vector<short> samples;
        bool prepared = false;
    };

    HWAVEOUT device{};
    std::array<Slot, 8> slots;
};

} // namespace pets
