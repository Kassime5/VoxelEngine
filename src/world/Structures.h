//
// Created by maxim on 23/08/2026.
//

#ifndef GLFWVOXEL_STRUCTURES_H
#define GLFWVOXEL_STRUCTURES_H

#include <cstdint>

#include "Block.h"
#include "PerlinNoise/PerlinNoise.hpp"

class ChunkBlockSink;
class WorleyBiome;

// Terrain height at any world column, including columns outside the chunk being built.
struct TerrainSampler {
    const WorleyBiome* biomes = nullptr;
    const siv::PerlinNoise* noise = nullptr;

    int heightAt(int worldX, int worldZ) const;
};

// Every emitter writes in world coordinates
namespace structures {

void tree(ChunkBlockSink& sink, int worldX, int surfaceY, int worldZ, uint32_t seed,
          int minHeight, int heightRange, int canopyRadius, int canopyDepth);

void fallenLog(ChunkBlockSink& sink, int worldX, int surfaceY, int worldZ, uint32_t seed,
               const TerrainSampler& terrain);

void cactus(ChunkBlockSink& sink, int worldX, int surfaceY, int worldZ, uint32_t seed);

void boulder(ChunkBlockSink& sink, int worldX, int surfaceY, int worldZ, uint32_t seed,
             const TerrainSampler& terrain);

} // namespace structures

#endif //GLFWVOXEL_STRUCTURES_H
