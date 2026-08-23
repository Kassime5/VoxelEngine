//
// Created by maxim on 18/01/2026.
//

#ifndef GLFWVOXEL_BIOME_H
#define GLFWVOXEL_BIOME_H

#include <memory>
#include <vector>
#include <glm/glm.hpp>
#include "Block.h"
#include <cstring>
#include "PerlinNoise/PerlinNoise.hpp"

#include "Structures.h"

class Chunk;
class ChunkBlockSink;
class World;

constexpr int SEA_LEVEL = 32;

struct Structure {
    std::string name;
    glm::ivec3 size;
    std::vector<BlockType> blocks;
    glm::ivec3 anchor;

    BlockType getBlock(int x, int y, int z) const {
        if (x < 0 || x >= size.x || y < 0 || y >= size.y || z < 0 || z >= size.z) {
            return BlockType::Air;
        }
        int index = x + y * size.x + z * size.x * size.y;
        if (index >= 0 && index < blocks.size()) {
            return blocks[index];
        }
        return BlockType::Air;
    }

    void setBlock(int x, int y, int z, BlockType type) {
        if (x < 0 || x >= size.x || y < 0 || y >= size.y || z < 0 || z >= size.z) {
            return;
        }
        int index = x + y * size.x + z * size.x * size.y;
        if (index >= 0 && index < blocks.size()) {
            blocks[index] = type;
        }
    }
};

class Biome {
public:
    virtual ~Biome() = default;

    virtual std::string getName() const { return name; }
    virtual uint8_t getBiomeId() const { return biomeId; }

    virtual float getBaseHeight(int worldX, int worldZ, const siv::PerlinNoise* noise) const = 0;
    virtual BlockType getSurfaceBlock(int worldX, int worldY, int worldZ) const { return surfaceBlock; }
    virtual BlockType getSubSurfaceBlock(int worldX, int worldY, int worldZ) const { return subSurfaceBlock; }
    virtual BlockType getStoneBlock(int worldX, int worldY, int worldZ) const { return stoneBlock; }

    // How many blocks of each layer
    virtual int getSurfaceDepth() const { return 1; }
    virtual int getSubSurfaceDepth() const { return 3; }

    // Structure gen
    virtual bool canSpawnStructures() const { return true; }
    virtual float getStructureSpawnChance() const { return 0.0f; }
    virtual std::vector<Structure> getStructures() const { return {}; }

    // Single-column decorations, which can never cross a chunk border.
    virtual void decorate(Chunk* chunk, int localX, int localZ, int surfaceY,
                         const siv::PerlinNoise* noise, uint32_t seed) const {}

    // Anything spanning more than its own column
    virtual void placeStructure(ChunkBlockSink& sink, int worldX, int surfaceY, int worldZ,
                                uint32_t seed, const TerrainSampler& terrain) const {}

    // Biome features
    virtual int getWaterLevel() const { return waterLevel; }
    virtual float getTemperature() const { return temperature; }
    virtual float getHumidity() const { return humidity; }
    virtual float getSpawnWeight() const { return spawnWeight; }

    // Transition blending with other biomes
    virtual bool canBlendWith(const Biome* other) const { return true; }
    virtual float getBlendFactor() const { return 1.0f; }

protected:
    float getHeightNoise(int worldX, int worldZ, const siv::PerlinNoise* noise, float frequency = 0.01f, int octaves = 4) const {
        return static_cast<float>(noise->octave2D_01(worldX * frequency, worldZ * frequency, octaves));
    }

    std::string name;
    uint8_t biomeId;

    int waterLevel = SEA_LEVEL;
    float temperature = 0.5f;
    float humidity = 0.5f;
    float spawnWeight = 1.0f;

    BlockType surfaceBlock = BlockType::Grass;
    BlockType subSurfaceBlock = BlockType::Dirt;
    BlockType stoneBlock = BlockType::Stone;

};

#endif //GLFWVOXEL_BIOME_H