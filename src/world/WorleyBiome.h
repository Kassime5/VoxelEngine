//
// Created by maxim on 18/01/2026.
//


#ifndef GLFWVOXEL_WORLEYBIOME_H
#define GLFWVOXEL_WORLEYBIOME_H

#include "Biome.h"
#include <glm/glm.hpp>
#include "PerlinNoise/PerlinNoise.hpp"

class WorleyBiome {
public:
    WorleyBiome(uint32_t seed, int regionSize = 512);

    // ChunkBiomeData generateChunkBiomeData(const glm::ivec3& chunkPos) const;

    const Biome* getBiomeAt(int x, int z) const;
    float getBlendedHeight(int x, int z, const siv::PerlinNoise* perlin) const;
    bool shouldSpawnStructure(const glm::ivec3& chunkPos, const Biome* biome) const;
    struct SpawnLocation {
        bool valid;
        glm::ivec3 position;
        float flatness;
    };
    SpawnLocation findSpawnLocation(const glm::ivec3& chunkPos, const Biome* biome,
                                   const std::function<int(int, int)>& getHeight) const;

private:
    uint32_t seed;
    int regionSize;

    float hash(int x, int z) const;
    uint32_t hashChunkPos(const glm::ivec3& pos) const;
    glm::vec2 getFeaturePoint(int gridX, int gridZ) const;
    const Biome* getBiomeForCell(int gridX, int gridZ) const;
    float getTotalBiomeWeight() const;
};

#endif //GLFWVOXEL_WORLEYBIOME_H