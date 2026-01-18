//
// Created by maxim on 18/01/2026.
//

#include "WorleyBiome.h"

#include "Block.h"
#include "PerlinNoise/PerlinNoise.hpp"

WorleyBiome::WorleyBiome(uint32_t seed, int regionSize)
    : seed(seed), regionSize(regionSize) {
    
    biomeConfigs.resize(4);
    biomeConfigs[(int) BiomeType::Forest] = {
        16.0f,32.0f,
        0.0f,
        BlockType::Grass,BlockType::Dirt,BlockType::Stone
    };
    biomeConfigs[(int) BiomeType::Desert] = {
        10.0f, 10.0f,
        0.0f,
        BlockType::Sand, BlockType::Sand, BlockType::Stone
    };
    biomeConfigs[(int) BiomeType::Mountain] = {
        20.0f, 80.0f,
        0.0f,
        BlockType::Snow, BlockType::Stone, BlockType::Stone
    };
    biomeConfigs[(int) BiomeType::Ocean] = {
        5.0f, 5.0f,
        0.0f,
        BlockType::Sand, BlockType::Sand, BlockType::Stone
    };
}

float WorleyBiome::hash(int x, int z) const {
    // Large prime numbers to minimize patterns
    uint32_t h = seed ^ (x * 73856093) ^ (z * 19349663);
    h = (h ^ 61) ^ (h >> 16);
    h += (h << 3);
    h ^= (h >> 4);
    h *= 0x27d4eb2d;
    h ^= (h >> 15);
    return static_cast<float>(h % 1000) / 1000.0f;
}

glm::vec2 WorleyBiome::getFeaturePoint(int gridX, int gridZ) const {
    float offsetX = hash(gridX, gridZ);
    float offsetZ = hash(gridZ, gridX);

    return glm::vec2(
        static_cast<float>(gridX) * regionSize + offsetX * regionSize,
        static_cast<float>(gridZ) * regionSize + offsetZ * regionSize
    );
}

BiomeType WorleyBiome::getBiomeForPoint(int gridX, int gridZ) const {
    float val = hash(gridX + 123, gridZ - 456) * biomeConfigs.size();
    return static_cast<BiomeType>(std::floor(val));
}

BiomeType WorleyBiome::getBiomeAt(int x, int z) const {
    int gridX = std::floor(static_cast<float>(x) / regionSize);
    int gridZ = std::floor(static_cast<float>(z) / regionSize);

    float minDist = 1e9;
    BiomeType closestBiome = BiomeType::Forest;

    // Check current cell and adjacent neighbors
    for (int i = -1; i <= 1; ++i) {
        for (int j = -1; j <= 1; ++j) {
            glm::vec2 point = getFeaturePoint(gridX + i, gridZ + j);
            float dist = glm::distance(point, glm::vec2(x, z));

            if (dist < minDist) {
                minDist = dist;
                closestBiome = getBiomeForPoint(gridX + i, gridZ + j);
            }
        }
    }

    return closestBiome;
}
const BiomeConfig& WorleyBiome::getConfigAt(int x, int z) const {
    return biomeConfigs[static_cast<int>(getBiomeAt(x, z))];
}

float WorleyBiome::getBlendedHeight(int x, int z, const siv::PerlinNoise* perlin) const {
    int gridX = std::floor(static_cast<float>(x) / regionSize);
    int gridZ = std::floor(static_cast<float>(z) / regionSize);

    float totalHeight = 0.0f;
    float totalWeight = 0.0f;

    // Sample all 9 neighboring feature points
    for (int i = -1; i <= 1; ++i) {
        for (int j = -1; j <= 1; ++j) {
            glm::vec2 point = getFeaturePoint(gridX + i, gridZ + j);
            float dist = glm::distance(point, glm::vec2(x, z));

            // Calculate a weight that drops off with distance
            float weight = 1.0f / (std::pow(dist, 3.0f) + 0.0001f);

            BiomeType type = getBiomeForPoint(gridX + i, gridZ + j);
            const BiomeConfig& config = biomeConfigs[(int)type];

            double noise = perlin->octave2D_01(x * 0.01, z * 0.01, 4);
            float height = config.heightOffset + static_cast<float>(noise) * config.heightScale;

            totalHeight += height * weight;
            totalWeight += weight;
        }
    }

    return totalHeight / totalWeight;
}
