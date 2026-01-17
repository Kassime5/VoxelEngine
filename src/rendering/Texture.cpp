//
// Created by maxim on 03/01/2026.
//

#include "Texture.h"
#define STB_IMAGE_IMPLEMENTATION
#include "../stb_image.h"
#include <iostream>

Texture::Texture()
    : id(0), width(0), height(0), channels(0), hasMipmaps(false) {
}

Texture::Texture(const char* path, bool generateMipmaps)
    : id(0), width(0), height(0), channels(0), hasMipmaps(false) {
    load(path, generateMipmaps);
}

Texture::Texture(const std::string& path, bool generateMipmaps)
    : Texture(path.c_str(), generateMipmaps) {
}

Texture::~Texture() {
    cleanup();
}

Texture::Texture(Texture&& other) noexcept
    : id(other.id),
      width(other.width),
      height(other.height),
      channels(other.channels),
      hasMipmaps(other.hasMipmaps) {
    other.id = 0;
    other.width = 0;
    other.height = 0;
    other.channels = 0;
    other.hasMipmaps = false;
}

Texture& Texture::operator=(Texture&& other) noexcept {
    if (this != &other) {
        cleanup();

        id = other.id;
        width = other.width;
        height = other.height;
        channels = other.channels;
        hasMipmaps = other.hasMipmaps;

        other.id = 0;
        other.width = 0;
        other.height = 0;
        other.channels = 0;
        other.hasMipmaps = false;
    }
    return *this;
}

bool Texture::load(const char* path, bool generateMipmaps) {
    cleanup();

    // Flip texture vertically (OpenGL expects 0,0 at bottom-left)
    stbi_set_flip_vertically_on_load(true);

    // Load image data
    unsigned char* data = stbi_load(path, &width, &height, &channels, 0);

    if (!data) {
        std::cerr << "Failed to load texture: " << path << std::endl;
        std::cerr << "STB Error: " << stbi_failure_reason() << std::endl;
        return false;
    }

    // Generate OpenGL texture
    glGenTextures(1, &id);
    glBindTexture(GL_TEXTURE_2D, id);

    // Set default texture parameters
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

    if (generateMipmaps) {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    } else {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    }

    // Determine format based on channel count
    GLenum format = determineFormat(channels);
    GLenum internalFormat = format;

    // Upload texture data
    glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, width, height, 0, format, GL_UNSIGNED_BYTE, data);

    // Generate mipmaps if requested
    if (generateMipmaps) {
        glGenerateMipmap(GL_TEXTURE_2D);
        hasMipmaps = true;
    }

    // Free image data
    stbi_image_free(data);

    // Unbind texture
    glBindTexture(GL_TEXTURE_2D, 0);

    std::cout << "Loaded texture: " << path
              << " (" << width << "x" << height << ", " << channels << " channels)" << std::endl;

    return true;
}

bool Texture::load(const std::string& path, bool generateMipmaps) {
    return load(path.c_str(), generateMipmaps);
}

void Texture::bind(unsigned int slot) const {
    if (id != 0) {
        glActiveTexture(GL_TEXTURE0 + slot);
        glBindTexture(GL_TEXTURE_2D, id);
    }
}

void Texture::unbind() const {
    glBindTexture(GL_TEXTURE_2D, 0);
}

void Texture::setWrapMode(WrapMode wrapS, WrapMode wrapT) {
    if (id == 0) return;

    glBindTexture(GL_TEXTURE_2D, id);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, static_cast<GLint>(wrapS));
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, static_cast<GLint>(wrapT));
    glBindTexture(GL_TEXTURE_2D, 0);
}

void Texture::setFilterMode(FilterMode minFilter, FilterMode magFilter) {
    if (id == 0) return;

    glBindTexture(GL_TEXTURE_2D, id);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, static_cast<GLint>(minFilter));
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, static_cast<GLint>(magFilter));
    glBindTexture(GL_TEXTURE_2D, 0);
}

void Texture::cleanup() {
    if (id != 0) {
        glDeleteTextures(1, &id);
        id = 0;
        width = 0;
        height = 0;
        channels = 0;
        hasMipmaps = false;
    }
}

GLenum Texture::determineFormat(int channels) const {
    switch (channels) {
        case 1: return GL_RED;
        case 2: return GL_RG;
        case 3: return GL_RGB;
        case 4: return GL_RGBA;
        default:
            std::cerr << "Unsupported number of channels: " << channels << std::endl;
            return GL_RGB;
    }
}

glm::vec2 Texture::getAtlasUV(int tileX, int tileY, int tilesPerRow, float u, float v) const {
    float tileSize = 1.0f / tilesPerRow;
    float offsetX = tileX * tileSize;
    float offsetY = tileY * tileSize;
    return glm::vec2(offsetX + u * tileSize, offsetY + v * tileSize);
}
