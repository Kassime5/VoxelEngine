//
// Created by maxim on 04/01/2026.
//

#include "TextureAltas.h"

#include "src/core/GL.h"
#include "../stb_image.h"

#include <algorithm>
#include <cstring>
#include <iostream>
#include <vector>

namespace {
    // Tiles stop being meaningful once a mip shrinks them to a couple of texels
    constexpr int MAX_MIP_LEVELS = 6;
    constexpr float MAX_ANISOTROPY = 8.0f;
}

TextureAtlas::~TextureAtlas() {
    cleanup();
}

bool TextureAtlas::load(const char* atlasPath, int _tilesPerRow) {
    cleanup();
    tilesPerRow = _tilesPerRow;

    // Flip vertically so tile 0 is the bottom-left of the sheet, matching the old UV maths
    stbi_set_flip_vertically_on_load(true);

    int width = 0, height = 0, channels = 0;
    unsigned char* data = stbi_load(atlasPath, &width, &height, &channels, 4);
    if (!data) {
        std::cerr << "Failed to load texture atlas: " << atlasPath << std::endl;
        std::cerr << "STB Error: " << stbi_failure_reason() << std::endl;
        return false;
    }

    const int tileSize = width / tilesPerRow;
    const int tileRows = tileSize > 0 ? height / tileSize : 0;
    if (tileSize <= 0 || tileRows <= 0 || width % tilesPerRow != 0) {
        std::cerr << "Atlas " << atlasPath << " is " << width << "x" << height
                  << ", not divisible into " << tilesPerRow << " square tiles per row" << std::endl;
        stbi_image_free(data);
        return false;
    }
    const int layerCount = tilesPerRow * tileRows;

    glGenTextures(1, &textureId);
    glBindTexture(GL_TEXTURE_2D_ARRAY, textureId);
    glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_RGBA8, tileSize, tileSize, layerCount,
                 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);


    std::vector<unsigned char> tile(static_cast<size_t>(tileSize) * tileSize * 4);
    const size_t tileRowBytes = static_cast<size_t>(tileSize) * 4;

    for (int layer = 0; layer < layerCount; ++layer) {
        const int originX = (layer % tilesPerRow) * tileSize;
        const int originY = (layer / tilesPerRow) * tileSize;

        for (int row = 0; row < tileSize; ++row) {
            const unsigned char* src =
                data + (static_cast<size_t>(originY + row) * width + originX) * 4;
            std::memcpy(tile.data() + static_cast<size_t>(row) * tileRowBytes, src, tileRowBytes);
        }

        glTexSubImage3D(GL_TEXTURE_2D_ARRAY, 0, 0, 0, layer, tileSize, tileSize, 1,
                        GL_RGBA, GL_UNSIGNED_BYTE, tile.data());
    }

    stbi_image_free(data);

    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAX_LEVEL, MAX_MIP_LEVELS);
    glGenerateMipmap(GL_TEXTURE_2D_ARRAY);

#ifndef __EMSCRIPTEN__
    GLfloat maxAnisotropy = 1.0f;
    glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY, &maxAnisotropy);
    glTexParameterf(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAX_ANISOTROPY,
                    std::min(maxAnisotropy, MAX_ANISOTROPY));
#endif

    glBindTexture(GL_TEXTURE_2D_ARRAY, 0);

    std::cout << "Loaded texture atlas: " << atlasPath << " (" << width << "x" << height
              << ", " << layerCount << " tiles of " << tileSize << "px)" << std::endl;

    return true;
}

void TextureAtlas::bind(unsigned int slot) const {
    if (textureId == 0) return;

    glActiveTexture(GL_TEXTURE0 + slot);
    glBindTexture(GL_TEXTURE_2D_ARRAY, textureId);
}

void TextureAtlas::cleanup() {
    if (textureId != 0) {
        glDeleteTextures(1, &textureId);
        textureId = 0;
    }
}

namespace {
    constexpr int WOOD_END_GRAIN_TILE = 0;
}

BlockTexture TextureAtlas::getBlockTexture(BlockType block) const {
    BlockTexture tex;

    switch (block) {
        case BlockType::Grass:
            tex.topTile = 86;
            tex.sideTile = 57;
            tex.bottomTile = 47;
            break;
        case BlockType::Dirt:
            tex.topTile = 47;
            tex.sideTile = 47;
            tex.bottomTile = 47;
            break;
        case BlockType::Stone:
            tex.topTile = 53;
            tex.sideTile = 53;
            tex.bottomTile = 53;
            break;
        case BlockType::Sand:
            tex.topTile = 33;
            tex.sideTile = 33;
            tex.bottomTile = 33;
            break;
        case BlockType::Snow:
            tex.topTile = 43;
            tex.sideTile = 43;
            tex.bottomTile = 43;
            break;
        case BlockType::Leaves:
            tex.topTile = 14;
            tex.sideTile = 14;
            tex.bottomTile = 14;
            break;
        case BlockType::Wood:
            tex.topTile = 0;
            tex.sideTile = 91;
            tex.bottomTile = 0;
            break;
        // Lying down: bark everywhere
        case BlockType::WoodX:
        case BlockType::WoodZ:
            tex.topTile = 91;
            tex.sideTile = 91;
            tex.bottomTile = 91;
            break;
        case BlockType::TallGrass:
            tex.topTile = 56;
            tex.sideTile = 56;
            tex.bottomTile = 56;
            break;
        case BlockType::Water:
            tex.topTile = 7;
            tex.sideTile = 7;
            tex.bottomTile = 7;
            break;
        case BlockType::Cactus:
            tex.topTile = 88;
            tex.sideTile = 88;
            tex.bottomTile = 88;
            break;
        default:
            tex.topTile = 0;
            tex.sideTile = 0;
            tex.bottomTile = 0;
            break;
    }

    return tex;
}

uint8_t TextureAtlas::getBlockFaceTileIndex(BlockType block, BlockFace face) const {
    BlockTexture tex = getBlockTexture(block);
    int tileIndex;

    // Show wood interior on the edges
    if (block == BlockType::WoodX && (face == BlockFace::Front || face == BlockFace::Back)) {
        return static_cast<uint8_t>(WOOD_END_GRAIN_TILE);
    }
    if (block == BlockType::WoodZ && (face == BlockFace::Right || face == BlockFace::Left)) {
        return static_cast<uint8_t>(WOOD_END_GRAIN_TILE);
    }

    switch (face) {
    case BlockFace::Top:
        tileIndex = tex.topTile;
        break;
    case BlockFace::Bottom:
        tileIndex = tex.bottomTile;
        break;
    default:
        tileIndex = tex.sideTile;
        break;
    }

    return static_cast<uint8_t>(tileIndex);
}
