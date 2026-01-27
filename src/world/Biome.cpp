//
// Created by maxim on 19/01/2026.
//

#include "Biome.h"
#include "Chunk.h"

class ForestBiome : public Biome {
public:
    ForestBiome() {
        name = "Forest";
        biomeId = 1;
        waterLevel = 32;
        temperature = 0.6f;
        humidity = 0.7f;
        spawnWeight = 40.0f;
    }

    float getBaseHeight(int worldX, int worldZ, const siv::PerlinNoise* noise) const {
        float baseNoise = getHeightNoise(worldX, worldZ, noise, 0.01f, 4);
        return 16.0f + baseNoise * 32.0f;
    }

    float getStructureSpawnChance() const override { return 0.15f; }

    void decorate(Chunk* chunk, int localX, int localZ, int surfaceY, const siv::PerlinNoise* noise, uint32_t seed) const {
        float treeChance = static_cast<float>((seed ^ 0x12345) % 1000) / 1000.0f;

        if (treeChance < 0.01f) { // 5% chance for a tree
            int treeHeight = 4 + (seed % 3); // Random height 4-6

            // Tree trunk
            for (int y = 0; y < treeHeight; y++) {
                if (surfaceY + y < Chunk::HEIGHT) {
                    chunk->setBlock(localX, surfaceY + y, localZ, BlockType::Wood);
                }
            }

            int leafY = surfaceY + treeHeight;
            for (int dx = -1; dx <= 1; dx++) {
                for (int dz = -1; dz <= 1; dz++) {
                    for (int dy = 0; dy < 2; dy++) {
                        int x = localX + dx;
                        int z = localZ + dz;
                        int y = leafY + dy;

                        if (x >= 0 && x < Chunk::SIZE &&
                            z >= 0 && z < Chunk::SIZE &&
                            y < Chunk::HEIGHT) {
                            chunk->setBlock(x, y, z, BlockType::Leaves);
                            }
                    }
                }
            }
        }
    }
};

class DesertBiome : public Biome {
public:
    DesertBiome() {
        name = "Desert";
        biomeId = 2;
        waterLevel = 32;
        temperature = 1.0f;
        humidity = 0.1f;
        spawnWeight = 15.0f;
        surfaceBlock = BlockType::Sand;
        subSurfaceBlock = BlockType::Sand;
        stoneBlock = BlockType::Sand;
    }

    float getBaseHeight(int worldX, int worldZ, const siv::PerlinNoise* noise) const {
        float baseNoise = getHeightNoise(worldX, worldZ, noise, 0.02f, 3);
        return 10.0f + baseNoise * 10.0f;
    }

    int getSubSurfaceDepth() const override { return 5; }

    float getStructureSpawnChance() const override { return 0.05f; }

    // void decorate(Chunk* chunk, int localX, int localZ, int surfaceY,
    //               const siv::PerlinNoise* noise, uint32_t seed) const override;
};

class MountainBiome : public Biome {
public:
    MountainBiome() {
        name = "Mountain";
        biomeId = 3;
        waterLevel = 32;
        temperature = 0.2f;
        humidity = 0.4f;
        spawnWeight = 25.0f;
        surfaceBlock = BlockType::Snow;
        subSurfaceBlock = BlockType::Stone;
        stoneBlock = BlockType::Stone;
    }

    float getBaseHeight(int worldX, int worldZ, const siv::PerlinNoise* noise) const {
        float baseNoise = getHeightNoise(worldX, worldZ, noise, 0.008f, 4);
        return 38.0f + baseNoise * 128.0f;
    }

    BlockType getSurfaceBlock(int worldX, int worldY, int worldZ) const override {
        if (worldY > 70) {
            return BlockType::Snow;
        }
        if (worldY > 50) {
            uint32_t hash = ((worldX * 73856093) ^ (worldZ * 19349663) ^ (worldY * 83492791));
            float variation = (hash % 1000) / 1000.0f;

            float stoneThreshold = (worldY - 50.0f) / 20.0f;
            stoneThreshold += (variation - 0.5f) * 0.3f;
            if (stoneThreshold > 0.6f) {
                return BlockType::Stone;
            }
            return BlockType::Grass;
        }
        return BlockType::Grass;
    }

    int getSurfaceDepth() const override { return 1; }
    int getSubSurfaceDepth() const override { return 2; }
    float getStructureSpawnChance() const override { return 0.02f; }
};

class OceanBiome : public Biome {
public:
    OceanBiome() {
        name = "Ocean";
        biomeId = 4;
        waterLevel = 32;
        temperature = 0.5f;
        humidity = 1.0f;
        spawnWeight = 1.0f;
        surfaceBlock = BlockType::Sand;
        subSurfaceBlock = BlockType::Sand;
        stoneBlock = BlockType::Sand;
    }

    float getBaseHeight(int worldX, int worldZ, const siv::PerlinNoise* noise) const {
        float baseNoise = getHeightNoise(worldX, worldZ, noise, 0.015f, 3);
        return 5.0f + baseNoise * 5.0f;
    }

    bool canSpawnStructures() const override { return false; }
};

class BiomeRegistry {
public:
    static BiomeRegistry& getInstance() {
        static BiomeRegistry instance;
        return instance;
    }

    void registerBiome(std::unique_ptr<Biome> biome) {
        biomes.push_back(std::move(biome));
    }

    const Biome* getBiome(uint8_t id) const {
        if (id < biomes.size())
        {
            return biomes[id].get();
        }
        return biomes[0].get();
    }

    const Biome* getBiomeByName(const char* name) const {
        for (const auto& biome : biomes) {
            if (strcmp(biome->getName().c_str(), name) == 0) {
                return biome.get();
            }
        }
        return nullptr;
    }

    size_t getBiomeCount() const { return biomes.size(); }

    const std::vector<std::unique_ptr<Biome>>& getAllBiomes() const {
        return biomes;
    }

    void clear() {
        biomes.clear();
    }

private:
    BiomeRegistry() {
        registerBiome(std::make_unique<ForestBiome>());
        registerBiome(std::make_unique<DesertBiome>());
        registerBiome(std::make_unique<MountainBiome>());
        registerBiome(std::make_unique<OceanBiome>());
    }

    std::vector<std::unique_ptr<Biome>> biomes;
};
