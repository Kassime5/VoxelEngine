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

inline const char* biomeTypeToString(BiomeType type) {
    switch (type) {
    case BiomeType::Forest:   return "Forest";
    case BiomeType::Desert:   return "Desert";
    case BiomeType::Mountain: return "Mountain";
    case BiomeType::Ocean:    return "Ocean";
    default:                  return "Unknown";
    }
}

struct BiomeConfig {
    float heightOffset;
    float heightScale;
    float temperature;
    float weight;

    BlockType surfaceBlock;
    BlockType subSurfaceBlock;
    BlockType stoneBlock;
};

#endif //GLFWVOXEL_BIOME_H