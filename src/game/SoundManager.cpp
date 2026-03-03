//
// Created by maxim on 27/01/2026.
//

#include "SoundManager.h"

#include "SoundManager.h"

#include <cstdint>
#include <fstream>
#include <cstring>

// WAV file header structure
struct WAVHeader {
    char riff[4];
    int32_t fileSize;
    char wave[4];
    char fmt[4];
    int32_t fmtSize;
    int16_t audioFormat;
    int16_t numChannels;
    int32_t sampleRate;
    int32_t byteRate;
    int16_t blockAlign;
    int16_t bitsPerSample;
    char data[4];
    int32_t dataSize;
};

SoundManager::SoundManager(Player& _player)
    : device(nullptr), context(nullptr), player(_player)
{}

SoundManager::~SoundManager() {
    shutdown();
}

bool SoundManager::initialize() {
    device = alcOpenDevice(nullptr);
    if (!device) {
        std::cerr << "Failed to open OpenAL device" << std::endl;
        return false;
    }

    context = alcCreateContext(device, nullptr);
    if (!context) {
        std::cerr << "Failed to create OpenAL context" << std::endl;
        alcCloseDevice(device);
        return false;
    }

    if (!alcMakeContextCurrent(context)) {
        std::cerr << "Failed to make OpenAL context current" << std::endl;
        alcDestroyContext(context);
        alcCloseDevice(device);
        return false;
    }

    alListener3f(AL_POSITION, 0.0f, 0.0f, 0.0f);
    alListener3f(AL_VELOCITY, 0.0f, 0.0f, 0.0f);
    ALfloat orientation[] = {0.0f, 0.0f, -1.0f, 0.0f, 1.0f, 0.0f};
    alListenerfv(AL_ORIENTATION, orientation);

    return true;
}

void SoundManager::shutdown() {
    stopAllSounds();
    for (ALuint source : sources) {
        alDeleteSources(1, &source);
    }
    sources.clear();

    for (auto& pair : buffers) {
        alDeleteBuffers(1, &pair.second);
    }
    buffers.clear();

    if (context) {
        alcMakeContextCurrent(nullptr);
        alcDestroyContext(context);
        context = nullptr;
    }

    if (device) {
        alcCloseDevice(device);
        device = nullptr;
    }
}

bool SoundManager::loadWAV(const std::string& filepath, ALenum& format, ALvoid*& data, ALsizei& size, ALsizei& freq) {
    std::ifstream file(filepath, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "Failed to open WAV file: " << filepath << std::endl;
        return false;
    }

    WAVHeader header;
    file.read(reinterpret_cast<char*>(&header), sizeof(WAVHeader));

    if (std::strncmp(header.riff, "RIFF", 4) != 0 ||
        std::strncmp(header.wave, "WAVE", 4) != 0) {
        std::cerr << "Not a valid WAV file: " << filepath << std::endl;
        return false;
    }

    if (header.numChannels == 1) {
        format = (header.bitsPerSample == 8) ? AL_FORMAT_MONO8 : AL_FORMAT_MONO16;
    } else if (header.numChannels == 2) {
        format = (header.bitsPerSample == 8) ? AL_FORMAT_STEREO8 : AL_FORMAT_STEREO16;
    } else {
        std::cerr << "Unsupported number of channels: " << header.numChannels << std::endl;
        return false;
    }

    // Read audio data
    size = header.dataSize;
    freq = header.sampleRate;
    data = new char[size];
    file.read(static_cast<char*>(data), size);
    file.close();

    return true;
}

ALuint SoundManager::loadSound(const std::string& filepath) {
    // Careful : If the file is stereo => Cannot position it in the world !

    // Check if already loaded
    auto it = buffers.find(filepath);
    if (it != buffers.end()) {
        return it->second;
    }

    ALenum format;
    ALvoid* data;
    ALsizei size;
    ALsizei freq;

    if (!loadWAV(filepath, format, data, size, freq)) {
        return 0;
    }

    ALuint buffer;
    alGenBuffers(1, &buffer);
    alBufferData(buffer, format, data, size, freq);

    delete[] static_cast<char*>(data);

    ALenum error = alGetError();
    if (error != AL_NO_ERROR) {
        std::cerr << "OpenAL error creating buffer: " << error << std::endl;
        alDeleteBuffers(1, &buffer);
        return 0;
    }

    buffers[filepath] = buffer;
    std::cout << "Loaded sound: " << filepath << " (Buffer ID: " << buffer << ")" << std::endl;

    return buffer;
}

ALuint SoundManager::createSource() {
    ALuint source;
    alGenSources(1, &source);

    alSourcef(source, AL_PITCH, 1.0f);
    alSourcef(source, AL_GAIN, 1.0f);
    alSource3f(source, AL_POSITION, 0.0f, 0.0f, 0.0f);
    alSource3f(source, AL_VELOCITY, 0.0f, 0.0f, 0.0f);
    alSourcei(source, AL_LOOPING, AL_FALSE);

    alSourcei(source, AL_SOURCE_RELATIVE, AL_TRUE); // Is relative to listener
    alSourcef(source, AL_ROLLOFF_FACTOR, 0.0f); // Distance attenuation

    sources.push_back(source);
    return source;
}

ALuint SoundManager::getAvailableSource() {
    for (ALuint source : sources) {
        ALint state;
        alGetSourcei(source, AL_SOURCE_STATE, &state);
        if (state != AL_PLAYING) {
            return source;
        }
    }

    return createSource();
}

ALuint SoundManager::playSound(ALuint buffer, bool loop, float volume) {
    if (buffer == 0) return 0;

    ALuint source = getAvailableSource();

    alSourcei(source, AL_BUFFER, buffer);
    alSourcei(source, AL_LOOPING, loop ? AL_TRUE : AL_FALSE);
    alSourcef(source, AL_GAIN, volume);
    alSourcePlay(source);

    return source;
}

ALuint SoundManager::playSound3D(ALuint buffer, float x, float y, float z, bool loop, float volume) {
    if (buffer == 0) return 0;

    ALuint source = getAvailableSource();

    // Configure for positional sound
    alSourcei(source, AL_SOURCE_RELATIVE, AL_FALSE);
    alSourcef(source, AL_ROLLOFF_FACTOR, 1.0f);
    alSourcef(source, AL_REFERENCE_DISTANCE, 5.0f); // Distance where sound is at peak
    alSourcef(source, AL_MAX_DISTANCE, 100.0f); // Max distance to hear it

    alSourcei(source, AL_BUFFER, buffer);
    alSourcei(source, AL_LOOPING, loop ? AL_TRUE : AL_FALSE);
    alSourcef(source, AL_GAIN, volume);
    alSource3f(source, AL_POSITION, x, y, z);
    alSourcePlay(source);

    return source;
}

void SoundManager::stopSound(ALuint source) {
    alSourceStop(source);
}

void SoundManager::stopAllSounds() {
    for (ALuint source : sources) {
        alSourceStop(source);
    }
}

void SoundManager::setListenerPosition(float x, float y, float z) {
    alListener3f(AL_POSITION, x, y, z);
}

void SoundManager::setListenerVelocity(float x, float y, float z) {
    alListener3f(AL_VELOCITY, x, y, z);
}

void SoundManager::setListenerOrientation(float atX, float atY, float atZ,
                                          float upX, float upY, float upZ) {
    ALfloat orientation[] = {atX, atY, atZ, upX, upY, upZ};
    alListenerfv(AL_ORIENTATION, orientation);
}


void SoundManager::setSourcePosition(ALuint source, float x, float y, float z) {
    alSource3f(source, AL_POSITION, x, y, z);
}

void SoundManager::setSourceVelocity(ALuint source, float x, float y, float z) {
    alSource3f(source, AL_VELOCITY, x, y, z);
}

void SoundManager::setSourceVolume(ALuint source, float volume) {
    alSourcef(source, AL_GAIN, volume);
}

void SoundManager::setSourcePitch(ALuint source, float pitch) {
    alSourcef(source, AL_PITCH, pitch);
}

bool SoundManager::isPlaying(ALuint source) {
    ALint state;
    alGetSourcei(source, AL_SOURCE_STATE, &state);
    return state == AL_PLAYING;
}

void SoundManager::update() {
    glm::vec3 pos = player.getPosition();
    glm::vec3 vel = player.getVelocity();
    glm::vec3 front = player.getFront();
    glm::vec3 up = player.getUp();

    this->setListenerPosition(pos.x, pos.y, pos.z);
    // Effect is kinda horrible TODO: remove or change? Maybe divide the velocity?
    // this->setListenerVelocity(vel.x, vel.y, vel.z);
    this->setListenerOrientation(front.x, front.y, front.z,up.x, up.y, up.z);
}