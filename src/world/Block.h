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
    TallGrass
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
        return BlockRenderType::Transparent;
    default:
        return BlockRenderType::Solid;
    }
}

inline bool isBlockOpaque(BlockType type) {
    return type != BlockType::Air;
}

inline int printBlockType(BlockType type) {
    return (int)type;
}

#endif //GLFWVOXEL_BLOCK_H