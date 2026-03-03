//
// Created by maxim on 27/01/2026.
//

#ifndef GLFWVOXEL_SOUNDMANAGER_H
#define GLFWVOXEL_SOUNDMANAGER_H


#include <AL/al.h>
#include <AL/alc.h>
#include <string>
#include <vector>
#include <unordered_map>
#include <iostream>
#include "Player.h"


class SoundManager {
public:
    SoundManager(Player& _player);
    ~SoundManager();

    bool initialize();
    void shutdown();


    ALuint loadSound(const std::string& filepath);
    ALuint playSound(ALuint buffer, bool loop = false, float volume = 1.0f);
    ALuint playSound3D(ALuint buffer, float x, float y, float z, bool loop = false, float volume = 1.0f);

    void stopSound(ALuint source);
    void stopAllSounds();


    void setListenerPosition(float x, float y, float z);
    void setListenerVelocity(float x, float y, float z);
    void setListenerOrientation(float atX, float atY, float atZ, float upX, float upY, float upZ);

    void setSourcePosition(ALuint source, float x, float y, float z);
    void setSourceVelocity(ALuint source, float x, float y, float z);
    void setSourceVolume(ALuint source, float volume);
    void setSourcePitch(ALuint source, float pitch);
    bool isPlaying(ALuint source);

    void update();
private:
    ALCdevice* device;
    ALCcontext* context;
    Player& player;

    std::vector<ALuint> sources;
    std::unordered_map<std::string, ALuint> buffers;

    // Helper to load WAV file
    bool loadWAV(const std::string& filepath, ALenum& format,ALvoid*& data, ALsizei& size, ALsizei& freq);

    ALuint getAvailableSource();
    ALuint createSource();
};



#endif //GLFWVOXEL_SOUNDMANAGER_H