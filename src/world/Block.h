//
// Created by maxim on 03/01/2026.
//

#ifndef GLFWVOXEL_BLOCK_H
#define GLFWVOXEL_BLOCK_H
#include <cstdint>

enum class BlockType : uint8_t {
    Air = 0,
    Grass,
    Dirt,
    Stone,
    Sand,
    Snow,
    Wood,
    Leaves,
    TallGrass,
    // Wood lying along X or Z
    WoodX,
    WoodZ,
    Water,
    Cactus
};

enum class BlockRenderType : uint8_t {
    Solid,
    CrossModel,
    Transparent
};


inline BlockRenderType getBlockRenderType(BlockType type) {
    switch (type) {
    case BlockType::TallGrass:
        return BlockRenderType::CrossModel;
    case BlockType::Leaves:
    case BlockType::Water:
        return BlockRenderType::Transparent;
    default:
        return BlockRenderType::Solid;
    }
}

// isGroundBlock == something can stand on
inline bool isGroundBlock(BlockType type) {
    switch (type) {
    case BlockType::Air:
    case BlockType::TallGrass:
    case BlockType::Leaves:
    case BlockType::Wood:
    case BlockType::WoodX:
    case BlockType::WoodZ:
    case BlockType::Water:
        return false;
    default:
        return true;
    }
}

inline bool canGoThrough(BlockType type) {
    return type == BlockType::Air || type == BlockType::TallGrass || type == BlockType::Water;
}

inline bool isBlockOpaque(BlockType type) {
    return type != BlockType::Air && getBlockRenderType(type) == BlockRenderType::Solid;
}

inline int printBlockType(BlockType type) {
    return (int)type;
}

inline bool canRaycastThrough(BlockType type) {
    return type == BlockType::Air || type == BlockType::Water;
}

#endif //GLFWVOXEL_BLOCK_H