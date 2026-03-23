#include "template.h"
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