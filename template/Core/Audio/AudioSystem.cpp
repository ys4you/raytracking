#include "template.h"

#ifndef CP_UTF8
#define CP_UTF8 65001
#endif

extern "C" {
    __declspec(dllimport) int __stdcall WideCharToMultiByte(
        unsigned int CodePage, unsigned long dwFlags,
        const wchar_t* lpWideCharStr, int cchWideChar,
        char* lpMultiByteStr, int cbMultiByte,
        const char* lpDefaultChar, int* lpUsedDefaultChar);
    __declspec(dllimport) int __stdcall MultiByteToWideChar(
        unsigned int CodePage, unsigned long dwFlags,
        const char* lpMultiByteStr, int cbMultiByte,
        wchar_t* lpWideCharStr, int cchWideChar);
}

// Disable backends you don't have / don't need
#define MA_NO_JACK
#define MA_NO_RUNTIME_LINKING

#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"

#include "AudioSystem.h"
#include <iostream>

AudioSystem& AudioSystem::Get()
{
    static AudioSystem instance;
    return instance;
}

bool AudioSystem::Init()
{
    if (ma_engine_init(NULL, &engine) != MA_SUCCESS)
    {
        std::cout << "Failed to init audio engine\n";
        return false;
    }

    return true;
}

void AudioSystem::Shutdown()
{
    for (auto& [key, sound] : sounds)
    {
        ma_sound_uninit(sound);
        delete sound;
    }

    sounds.clear();
    ma_engine_uninit(&engine);
}

void AudioSystem::Play(const std::string& file, bool loop)
{
    ma_sound* sound = nullptr;

    // Reuse if already loaded
    if (sounds.find(file) == sounds.end())
    {
        sound = new ma_sound();

        if (ma_sound_init_from_file(&engine, file.c_str(), 0, NULL, NULL, sound) != MA_SUCCESS)
        {
            std::cout << "Failed to load sound: " << file << "\n";
            delete sound;
            return;
        }

        sounds[file] = sound;
    }
    else
    {
        sound = sounds[file];
    }

    ma_sound_set_looping(sound, loop);
    ma_sound_start(sound);
}

void AudioSystem::StopAll()
{
    for (auto& [key, sound] : sounds)
    {
        ma_sound_stop(sound);
    }
}

void AudioSystem::SetMasterVolume(float volume)
{
    ma_engine_set_volume(&engine, volume);
}