//
// Created by maxim on 03/01/2026.
//

#ifndef GLFWVOXEL_TEXTURE_H
#define GLFWVOXEL_TEXTURE_H

#include <glad/glad.h>
#include <string>

#include "glm/vec2.hpp"

class Texture {
public:
    unsigned int id;
    int width, height;
    int channels;

    // Texture parameters
    enum class WrapMode {
        Repeat = GL_REPEAT,
        MirroredRepeat = GL_MIRRORED_REPEAT,
        ClampToEdge = GL_CLAMP_TO_EDGE,
        ClampToBorder = GL_CLAMP_TO_BORDER
    };

    enum class FilterMode {
        Nearest = GL_NEAREST,
        Linear = GL_LINEAR,
        NearestMipmapNearest = GL_NEAREST_MIPMAP_NEAREST,
        LinearMipmapNearest = GL_LINEAR_MIPMAP_NEAREST,
        NearestMipmapLinear = GL_NEAREST_MIPMAP_LINEAR,
        LinearMipmapLinear = GL_LINEAR_MIPMAP_LINEAR
    };

    // Constructors
    Texture();
    Texture(const char* path, bool generateMipmaps = true);
    Texture(const std::string& path, bool generateMipmaps = true);
    ~Texture();

    // Delete copy constructor/assignment
    Texture(const Texture&) = delete;
    Texture& operator=(const Texture&) = delete;

    // Move semantics
    Texture(Texture&& other) noexcept;
    Texture& operator=(Texture&& other) noexcept;

    // Load texture from file
    bool load(const char* path, bool generateMipmaps = true);
    bool load(const std::string& path, bool generateMipmaps = true);

    // Bind/unbind
    void bind(unsigned int slot = 0) const;
    void unbind() const;

    // Set texture parameters
    void setWrapMode(WrapMode wrapS, WrapMode wrapT);
    void setFilterMode(FilterMode minFilter, FilterMode magFilter);

    // Utility
    bool isValid() const { return id != 0; }
    unsigned int getID() const { return id; }

    glm::vec2 getAtlasUV(int tileX, int tileY, int tilesPerRow, float u, float v) const;

private:
    bool hasMipmaps;

    void cleanup();
    GLenum determineFormat(int channels) const;
};

#endif //GLFWVOXEL_TEXTURE_H