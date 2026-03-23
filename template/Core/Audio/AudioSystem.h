#pragma once

#include <string>
#include <unordered_map>

#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"

class AudioSystem
{
public:
    static AudioSystem& Get();

    bool Init();
    void Shutdown();

    void Play(const std::string& file, bool loop = false);
    void StopAll();

    void SetMasterVolume(float volume);

private:
    AudioSystem() = default;
    ~AudioSystem() = default;

    ma_engine engine;
    std::unordered_map<std::string, ma_sound*> sounds;
};