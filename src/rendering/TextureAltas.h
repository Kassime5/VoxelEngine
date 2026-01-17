//
// Created by maxim on 04/01/2026.
//


#ifndef GLFWVOXEL_TEXTUREATLAS_H
#define GLFWVOXEL_TEXTUREATLAS_H

#include "Texture.h"
#include "../world/Block.h"
#include <glm/glm.hpp>
#include <array>

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
    TextureAtlas();
    ~TextureAtlas() = default;

    bool load(const char* atlasPath, int tilesPerRow = 16);
    void bind(unsigned int slot = 0) const;

    // Get UV coordinates for a specific block face
    std::array<glm::vec2, 4> getBlockFaceUVs(BlockType block, BlockFace face) const;

    bool isLoaded() const { return atlas.isValid(); }
    int getTilesPerRow() const { return tilesPerRow; }

    uint8_t getBlockFaceTileIndex(BlockType block, BlockFace face) const;
private:
    Texture atlas;
    int tilesPerRow;
    std::array<glm::vec2, 4> getTileUVs(int tileX, int tileY) const;

    // Block texture definitions
    BlockTexture getBlockTexture(BlockType block) const;
};

#endif //GLFWVOXEL_TEXTUREATLAS_H