#pragma once
#include <string>
#include <unordered_map>
#include "miniaudio.h"

class AudioSystem
{
public:
    static AudioSystem& Get();
    bool Init();
    void Shutdown();
    void Play(const std::string& file, bool loop = false);

    ma_sound* GetSound(const std::string& file)
	{
        auto it = sounds.find(file);
        return (it != sounds.end()) ? it->second : nullptr;
    }

    void StopAll();
    void SetMasterVolume(float volume);
private:
    AudioSystem() = default;
    ~AudioSystem() = default;
    ma_engine engine;
    std::unordered_map<std::string, ma_sound*> sounds;

};