//
// Created by maxim on 23/08/2026.
//

#include "Structures.h"

#include <algorithm>
#include <cstdlib>

#include "Chunk.h"
#include "WorleyBiome.h"

int TerrainSampler::heightAt(int worldX, int worldZ) const {
    if (!biomes || !noise) {
        return 0;
    }
    return static_cast<int>(biomes->getBlendedHeight(worldX, worldZ, noise));
}

namespace {

uint32_t mix(uint32_t seed, uint32_t salt) {
    uint32_t h = seed ^ salt;
    h ^= h >> 15;
    h *= 0x2c1b3c6du;
    h ^= h >> 12;
    return h;
}

constexpr int LOG_MIN_LENGTH = 3;
constexpr int LOG_LENGTH_RANGE = 3;   // 3..5
constexpr int LOG_MAX_REACH = LOG_MIN_LENGTH + LOG_LENGTH_RANGE - 2;
// A log only tolerates this much rise across its span before it would float or bury.
constexpr int LOG_MAX_SLOPE = 1;

constexpr int CACTUS_MIN_HEIGHT = 3;
constexpr int CACTUS_HEIGHT_RANGE = 2;   // 3..4
constexpr int CACTUS_ARM_REACH = 1;

constexpr int BOULDER_MAX_RADIUS = 2;

static_assert(LOG_MAX_REACH <= Chunk::STRUCTURE_MARGIN,
              "fallen logs reach further than chunks scan, so they would clip at borders");
static_assert(CACTUS_ARM_REACH <= Chunk::STRUCTURE_MARGIN,
              "cactus arms reach further than chunks scan");
static_assert(BOULDER_MAX_RADIUS <= Chunk::STRUCTURE_MARGIN,
              "boulders reach further than chunks scan");

} // namespace

namespace structures {

void tree(ChunkBlockSink& sink, int worldX, int surfaceY, int worldZ, uint32_t seed,
          int minHeight, int heightRange, int canopyRadius, int canopyDepth) {
    const int trunkHeight = minHeight + static_cast<int>(mix(seed, 0xA1u) % heightRange);

    for (int y = 0; y < trunkHeight; y++) {
        sink.set(worldX, surfaceY + y, worldZ, BlockType::Wood);
    }

    const int leafY = surfaceY + trunkHeight;
    for (int dx = -canopyRadius; dx <= canopyRadius; dx++) {
        for (int dz = -canopyRadius; dz <= canopyRadius; dz++) {
            // Clip the corners of anything wider than a single block, so a big canopy reads
            // as round rather than as a cube sitting on a stick.
            if (canopyRadius > 1 && std::abs(dx) == canopyRadius && std::abs(dz) == canopyRadius) {
                continue;
            }
            for (int dy = 0; dy < canopyDepth; dy++) {
                sink.set(worldX + dx, leafY + dy, worldZ + dz, BlockType::Leaves);
            }
        }
    }
}

void fallenLog(ChunkBlockSink& sink, int worldX, int surfaceY, int worldZ, uint32_t seed,
               const TerrainSampler& terrain) {
    const bool alongX = (mix(seed, 0xB2u) & 1u) != 0;
    const int length = LOG_MIN_LENGTH + static_cast<int>(mix(seed, 0xB3u) % LOG_LENGTH_RANGE);
    const BlockType log = alongX ? BlockType::WoodX : BlockType::WoodZ;

    // Reject slopes
    int minY = surfaceY;
    int maxY = surfaceY;
    for (int i = 0; i < length; i++) {
        const int h = terrain.heightAt(worldX + (alongX ? i : 0), worldZ + (alongX ? 0 : i));
        minY = std::min(minY, h);
        maxY = std::max(maxY, h);
    }
    if (maxY - minY > LOG_MAX_SLOPE) {
        return;
    }

    // Sit on the highest column so no segment is buried
    for (int i = 0; i < length; i++) {
        sink.set(worldX + (alongX ? i : 0), maxY, worldZ + (alongX ? 0 : i), log);
    }
}

void cactus(ChunkBlockSink& sink, int worldX, int surfaceY, int worldZ, uint32_t seed) {
    const int height = CACTUS_MIN_HEIGHT + static_cast<int>(mix(seed, 0xC4u) % CACTUS_HEIGHT_RANGE);

    for (int y = 0; y < height; y++) {
        sink.set(worldX, surfaceY + y, worldZ, BlockType::Cactus);
    }

    // chance to grow another arm
    if (mix(seed, 0xC5u) % 3u != 0u || height < CACTUS_MIN_HEIGHT + 1) {
        return;
    }

    const uint32_t dir = mix(seed, 0xC6u) % 4u;
    const int armX = (dir == 0) ? CACTUS_ARM_REACH : (dir == 1 ? -CACTUS_ARM_REACH : 0);
    const int armZ = (dir == 2) ? CACTUS_ARM_REACH : (dir == 3 ? -CACTUS_ARM_REACH : 0);
    const int armY = surfaceY + height - 2;

    sink.set(worldX + armX, armY, worldZ + armZ, BlockType::Cactus);
    sink.set(worldX + armX, armY + 1, worldZ + armZ, BlockType::Cactus);
}

void boulder(ChunkBlockSink& sink, int worldX, int surfaceY, int worldZ, uint32_t seed,
             const TerrainSampler& terrain) {
    const int radius = 1 + static_cast<int>(mix(seed, 0xD7u) % BOULDER_MAX_RADIUS);

    for (int dx = -radius; dx <= radius; dx++) {
        for (int dz = -radius; dz <= radius; dz++) {
            if (std::abs(dx) + std::abs(dz) > radius) {
                continue;
            }

            // Each column is dropped onto its own ground
            const int base = terrain.heightAt(worldX + dx, worldZ + dz);
            const int top = radius - (std::abs(dx) + std::abs(dz)) / 2;

            for (int dy = 0; dy <= top; dy++) {
                sink.set(worldX + dx, base + dy, worldZ + dz, BlockType::Stone);
            }
        }
    }
}

} // namespace structures
