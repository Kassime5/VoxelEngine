//
// Created by maxim on 04/01/2026.
//

#include "TextureAltas.h"

#include <iostream>
#include <ostream>

TextureAtlas::TextureAtlas() : tilesPerRow(8) {
}

bool TextureAtlas::load(const char* atlasPath, int tilesPerRow) {
    tilesPerRow = tilesPerRow;

    bool success = atlas.load(atlasPath);
    if (success) {
        // Set texture parameters for pixel art style
        atlas.setFilterMode(Texture::FilterMode::Nearest, Texture::FilterMode::Nearest);
        atlas.setWrapMode(Texture::WrapMode::Repeat, Texture::WrapMode::Repeat);
    }

    return success;
}

void TextureAtlas::bind(unsigned int slot) const {
    atlas.bind(slot);
}

BlockTexture TextureAtlas::getBlockTexture(BlockType block) const {
    BlockTexture tex;

    switch (block) {
        case BlockType::Grass:
            tex.topTile = 1;
            tex.sideTile = 2;
            tex.bottomTile = 3;
            break;

        case BlockType::Dirt:
            tex.topTile = 3;
            tex.sideTile = 3;
            tex.bottomTile = 3;
            break;

        case BlockType::Stone:
            tex.topTile = 4;
            tex.sideTile = 4;
            tex.bottomTile = 4;
            break;

        case BlockType::Sand:
            tex.topTile = 5;
            tex.sideTile = 5;
            tex.bottomTile = 5;
            break;

        case BlockType::Snow:
            tex.topTile = 6;
            tex.sideTile = 6;
            tex.bottomTile = 6;
            break;

        default:
            tex.topTile = 0;
            tex.sideTile = 0;
            tex.bottomTile = 0;
            break;
    }

    return tex;
}

std::array<glm::vec2, 4> TextureAtlas::getTileUVs(int tileX, int tileY) const {
    float tileSize = 1.0f / tilesPerRow;
    float u0 = tileX * tileSize;
    float v0 = tileY * tileSize;
    float u1 = u0 + tileSize;
    float v1 = v0 + tileSize;

    // Return UVs in order: bottom-left, bottom-right, top-right, top-left
    return {{
        {u0, v0},  // Bottom-left
        {u1, v0},  // Bottom-right
        {u1, v1},  // Top-right
        {u0, v1}   // Top-left
    }};
}

std::array<glm::vec2, 4> TextureAtlas::getBlockFaceUVs(BlockType block, BlockFace face) const {
    BlockTexture blockTex = getBlockTexture(block);

    int tileIndex;

    switch (face) {
        case BlockFace::Top:
            tileIndex = blockTex.topTile;
            break;
        case BlockFace::Bottom:
            tileIndex = blockTex.bottomTile;
            break;
        default:
            tileIndex = blockTex.sideTile;
            break;
    }

    int tileX = tileIndex % tilesPerRow;
    int tileY = tileIndex / tilesPerRow;

    return getTileUVs(tileX, tileY);
}

uint8_t TextureAtlas::getBlockFaceTileIndex(BlockType block, BlockFace face) const {
    BlockTexture tex = getBlockTexture(block);
    int tileIndex;

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
