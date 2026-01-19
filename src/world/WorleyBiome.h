//
// Created by maxim on 18/01/2026.
//

#ifndef GLFWVOXEL_WORLEYBIOME_H
#define GLFWVOXEL_WORLEYBIOME_H

#include "Biome.h"
#include <glm/glm.hpp>
#include <vector>

#include "PerlinNoise/PerlinNoise.hpp"

struct BiomePoint {
    glm::vec2 position;
    BiomeType type;
};

struct BiomeBlendInfo {
    const BiomeConfig& c1;
    const BiomeConfig& c2;
    float weight;
};

class WorleyBiome {
public:
    WorleyBiome(uint32_t seed, int regionSize = 256);
    BiomeType getBiomeAt(int x, int z) const;
    const BiomeConfig& getConfigAt(int x, int z) const;
    float getBlendedHeight(int x, int z, const siv::PerlinNoise* perlin) const;
    float getTotalBiomeWeight() const;

private:
    uint32_t seed;
    int regionSize;
    std::vector<BiomeConfig> biomeConfigs;

    float hash(int x, int z) const;
    glm::vec2 getFeaturePoint(int gridX, int gridZ) const;
    BiomeType getBiomeForPoint(int gridX, int gridZ, float biomeWeight) const;
};


#endif //GLFWVOXEL_WORLEYBIOME_H