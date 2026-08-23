//
// Created by maxim on 04/01/2026.
//


#ifndef GLFWVOXEL_TEXTUREATLAS_H
#define GLFWVOXEL_TEXTUREATLAS_H

#include "../world/Block.h"
#include <cstdint>

enum class BlockFace {
    Front = 0,
    Back = 1,
    Top = 2,
    Bottom = 3,
    Right = 4,
    Left = 5
};

struct BlockTexture {
    int topTile = 0;
    int bottomTile = 0;
    int sideTile = 0;
};

class TextureAtlas {
public:
    TextureAtlas() = default;
    ~TextureAtlas();

    TextureAtlas(const TextureAtlas&) = delete;
    TextureAtlas& operator=(const TextureAtlas&) = delete;

    bool load(const char* atlasPath, int _tilesPerRow = 16);
    void bind(unsigned int slot = 0) const;

    bool isLoaded() const { return textureId != 0; }
    unsigned int getTextureId() const { return textureId; }

    uint8_t getBlockFaceTileIndex(BlockType block, BlockFace face) const;

private:
    unsigned int textureId = 0;
    int tilesPerRow = 8;

    void cleanup();

    // Block texture definitions
    BlockTexture getBlockTexture(BlockType block) const;
};

#endif //GLFWVOXEL_TEXTUREATLAS_H
