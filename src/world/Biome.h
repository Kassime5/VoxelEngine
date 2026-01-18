//
// Created by maxim on 18/01/2026.
//

#ifndef GLFWVOXEL_BIOME_H
#define GLFWVOXEL_BIOME_H
#include <cstdint>

enum class BlockType : uint8_t;

enum class BiomeType : uint8_t {
    Forest = 0,
    Desert,
    Mountain,
    Ocean
};

struct BiomeConfig {
    float heightOffset;
    float heightScale;

    // TODO: Use
    float temperature;
    BlockType surfaceBlock;
    BlockType subSurfaceBlock;
    BlockType stoneBlock;
};

#endif //GLFWVOXEL_BIOME_H