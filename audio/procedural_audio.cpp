#include "procedural_audio.hpp"

#include "../core/math.hpp"

#include <algorithm>
#include <cmath>

#pragma comment(lib, "winmm.lib")

namespace pets {

Audio::Audio() {
    WAVEFORMATEX format{};
    format.wFormatTag = WAVE_FORMAT_PCM;
    format.nChannels = 1;
    format.nSamplesPerSec = sampleRate;
    format.wBitsPerSample = 16;
    format.nBlockAlign = 2;
    format.nAvgBytesPerSec = sampleRate * 2;

    waveOutOpen(
        &device, WAVE_MAPPER, &format,
        0, 0, CALLBACK_NULL);
}

Audio::~Audio() {
    if (!device) return;

    waveOutReset(device);

    for (Slot& slot : slots) {
        if (slot.prepared)
            waveOutUnprepareHeader(device, &slot.header, sizeof(WAVEHDR));
    }

    waveOutClose(device);
}

void Audio::play(int type, double frequency, double volume) {
    if (!enabled || !device) return;

    Slot* available = nullptr;

    for (Slot& slot : slots) {
        if (slot.prepared &&
            (slot.header.dwFlags & WHDR_DONE))
        {
            waveOutUnprepareHeader(
                device, &slot.header, sizeof(WAVEHDR));

            slot.prepared = false;
        }

        if (!slot.prepared) {
            available = &slot;
            break;
        }
    }

    if (!available) return;

    Slot& slot = *available;
    double duration = type == 0 ? 0.075 : 0.22;

    slot.samples.resize(
        static_cast<size_t>(sampleRate * duration));

    for (size_t i = 0; i < slot.samples.size(); ++i) {
        double t = static_cast<double>(i) / sampleRate;
        double attack = std::min(1.0, t / 0.006);
        double decay = std::exp(-t * (type == 0 ? 58 : 17));
        double wave = std::sin(TAU * frequency * t);

        if (type == 0) {
            wave = wave * 0.45 +
                std::sin(TAU * frequency * 2.71 * t) * 0.32 +
                std::sin(TAU * frequency * 5.09 * t) * 0.18;
        } else if (type == 1) {
            wave = wave * 0.8 +
                std::sin(TAU * frequency * 2 * t) * 0.16;
        } else {
            wave = wave * 0.65 +
                std::sin(TAU * frequency * 3 * t) * 0.22;
        }

        slot.samples[i] = static_cast<short>(
            std::clamp(wave * attack * decay * volume, -1.0, 1.0)
            * 32767);
    }

    slot.header = {};
    slot.header.lpData =
        reinterpret_cast<LPSTR>(slot.samples.data());

    slot.header.dwBufferLength =
        static_cast<DWORD>(slot.samples.size() * sizeof(short));

    if (waveOutPrepareHeader(
            device, &slot.header, sizeof(WAVEHDR)) == MMSYSERR_NOERROR)
    {
        slot.prepared = true;
        waveOutWrite(device, &slot.header, sizeof(WAVEHDR));
    }
}

} // namespace pets
