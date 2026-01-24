//
// Created by maxim on 18/01/2026.
//

#include "WorleyBiome.h"
#include "Biome.cpp"
#include "PerlinNoise/PerlinNoise.hpp"
#include <unordered_map>
#include <cmath>

// WorleyBiome implementation
WorleyBiome::WorleyBiome(uint32_t seed, int regionSize)
    : seed(seed), regionSize(regionSize) {
}

const Biome* WorleyBiome::getBiomeAt(int x, int z) const {
    int gridX = static_cast<int>(std::floor(static_cast<float>(x) / regionSize));
    int gridZ = static_cast<int>(std::floor(static_cast<float>(z) / regionSize));

    float minDist = 1e9f;
    const Biome* closestBiome = nullptr;

    float totalWeight = getTotalBiomeWeight();
    for (int i = -2; i <= 2; ++i) {
        for (int j = -2; j <= 2; ++j) {
            glm::vec2 point = getFeaturePoint(gridX + i, gridZ + j);
            float dist = glm::distance(point, glm::vec2(x, z));

            if (dist < minDist) {
                minDist = dist;
                closestBiome = getBiomeForCell(gridX + i, gridZ + j);
            }
        }
    }

    return closestBiome;
}

float WorleyBiome::getBlendedHeight(int x, int z, const siv::PerlinNoise* perlin) const {
    int gridX = std::floor(static_cast<float>(x) / regionSize);
    int gridZ = std::floor(static_cast<float>(z) / regionSize);

    float totalHeight = 0.0f;
    float totalWeight = 0.0f;

    for (int i = -2; i <= 2; ++i) {
        for (int j = -2; j <= 2; ++j) {
            glm::vec2 point = getFeaturePoint(gridX + i, gridZ + j);
            float dist = glm::distance(point, glm::vec2(x, z));

            float weight = 1.0f / (std::pow(dist, 2.0f) + 1.0f);

            const Biome* biome = getBiomeForCell(gridX + i, gridZ + j);
            float height = biome->getBaseHeight(x, z, perlin);

            weight *= biome->getBlendFactor();

            totalHeight += height * weight;
            totalWeight += weight;
        }
    }

    return totalHeight / totalWeight;
}

bool WorleyBiome::shouldSpawnStructure(const glm::ivec3& chunkPos, const Biome* biome) const {
    if (!biome || !biome->canSpawnStructures()) {
        return false;
    }

    uint32_t h = hashChunkPos(chunkPos);
    float roll = static_cast<float>(h % 10000) / 10000.0f;

    return roll < biome->getStructureSpawnChance();
}

WorleyBiome::SpawnLocation WorleyBiome::findSpawnLocation(
    const glm::ivec3& chunkPos,
    const Biome* biome,
    const std::function<int(int, int)>& getHeight) const {

    SpawnLocation result{false, glm::ivec3(0), 0.0f};

    uint32_t h = hashChunkPos(chunkPos);

    // Try multiple locations
    for (int attempt = 0; attempt < 8; attempt++) {
        int localX = ((h ^ (attempt * 31)) % 48) + 8; // Keep from edges (8-56)
        int localZ = ((h ^ (attempt * 17)) % 48) + 8;

        int centerHeight = getHeight(localX, localZ);

        // Check flatness in 3x3 area
        int minH = centerHeight;
        int maxH = centerHeight;

        for (int dx = -2; dx <= 2; dx++) {
            for (int dz = -2; dz <= 2; dz++) {
                int x = localX + dx;
                int z = localZ + dz;

                // Check bounds
                if (x >= 0 && x < 64 && z >= 0 && z < 64) {
                    int h = getHeight(x, z);
                    minH = std::min(minH, h);
                    maxH = std::max(maxH, h);
                }
            }
        }

        int variation = maxH - minH;
        float flatness = 1.0f - (variation / 10.0f); // 0 = very rough, 1 = flat

        // Accept if relatively flat and better than previous
        if (variation <= 3 && flatness > result.flatness) {
            result.valid = true;
            result.position = glm::ivec3(localX, centerHeight, localZ);
            result.flatness = flatness;
        }
    }

    return result;
}

float WorleyBiome::hash(int x, int z) const {
    uint32_t h = seed ^ (x * 73856093) ^ (z * 19349663);
    h = (h ^ 61) ^ (h >> 16);
    h += (h << 3);
    h ^= (h >> 4);
    h *= 0x27d4eb2d;
    h ^= (h >> 15);
    return static_cast<float>(h % 1000) / 1000.0f;
}

uint32_t WorleyBiome::hashChunkPos(const glm::ivec3& pos) const {
    uint32_t h = seed ^ (pos.x * 73856093) ^ (pos.z * 19349663);
    h = (h ^ 61) ^ (h >> 16);
    h += (h << 3);
    h ^= (h >> 4);
    h *= 0x27d4eb2d;
    h ^= (h >> 15);
    return h;
}

glm::vec2 WorleyBiome::getFeaturePoint(int gridX, int gridZ) const {
    float offsetX = hash(gridX, gridZ);
    float offsetZ = hash(gridZ, gridX);

    return glm::vec2(
        static_cast<float>(gridX) * regionSize + offsetX * regionSize,
        static_cast<float>(gridZ) * regionSize + offsetZ * regionSize
    );
}

const Biome* WorleyBiome::getBiomeForCell(int gridX, int gridZ) const {
    float val = hash(gridX + 123, gridZ - 456);

    // Calculate total weight from BiomeRegistry
    float totalWeight = 0.0f;
    const auto& biomes = BiomeRegistry::getInstance().getAllBiomes();
    for (const auto& biome : biomes) {
        totalWeight += biome->getSpawnWeight();
    }

    // Select biome based on weighted random
    float target = val * totalWeight;
    float cumulative = 0.0f;

    for (const auto& biome : biomes) {
        cumulative += biome->getSpawnWeight();
        if (target <= cumulative) {
            return biome.get();
        }
    }

    return biomes[0].get();
}

float WorleyBiome::getTotalBiomeWeight() const {
    float totalWeight = 0.0f;
    const auto& biomes = BiomeRegistry::getInstance().getAllBiomes();
    for (const auto& biome : biomes) {
        totalWeight += biome->getSpawnWeight();
    }
    return totalWeight;
}

