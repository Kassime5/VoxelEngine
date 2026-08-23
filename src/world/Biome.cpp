//
// Created by maxim on 19/01/2026.
//

#include "Biome.h"
#include "Chunk.h"
#include "Structures.h"

class ForestBiome : public Biome {
public:
    ForestBiome() {
        name = "Forest";
        biomeId = 1;
        temperature = 0.6f;
        humidity = 0.7f;
        spawnWeight = 40.0f;
    }

    // Floors sit above SEA_LEVEL so only the blend into an ocean cell puts land underwater.
    float getBaseHeight(int worldX, int worldZ, const siv::PerlinNoise* noise) const {
        float baseNoise = getHeightNoise(worldX, worldZ, noise, 0.01f, 4);
        return SEA_LEVEL + 2.0f + baseNoise * 32.0f;
    }

    float getStructureSpawnChance() const override { return 0.15f; }

    void decorate(Chunk* chunk, int localX, int localZ, int surfaceY,
                  const siv::PerlinNoise* noise, uint32_t seed) const override {
        if (featureFor(seed) != Feature::Grass) {
            return;
        }

        if (surfaceY < Chunk::HEIGHT &&
            chunk->getBlock(localX, surfaceY - 1, localZ) == BlockType::Grass) {
            chunk->setBlock(localX, surfaceY, localZ, BlockType::TallGrass);
        }
    }

    void placeStructure(ChunkBlockSink& sink, int worldX, int surfaceY, int worldZ,
                        uint32_t seed, const TerrainSampler& terrain) const override {
        switch (featureFor(seed)) {
        case Feature::Oak:
            structures::tree(sink, worldX, surfaceY, worldZ, seed, 4, 3, 1, 2);
            break;
        case Feature::TallTree:
            structures::tree(sink, worldX, surfaceY, worldZ, seed, 7, 4, 1, 3);
            break;
        case Feature::BushyTree:
            structures::tree(sink, worldX, surfaceY, worldZ, seed, 3, 2, 2, 2);
            break;
        case Feature::FallenLog:
            structures::fallenLog(sink, worldX, surfaceY, worldZ, seed, terrain);
            break;
        default:
            break;
        }
    }

private:
    enum class Feature { None, Grass, Oak, TallTree, BushyTree, FallenLog };

    // One roll cut into disjoint bands. decorate() and placeStructure() both run over this
    // column and have to reach the same verdict, so they share the draw rather than rolling
    // separately and hoping the two agree.
    Feature featureFor(uint32_t seed) const {
        const float r = roll(seed, 0x54321u);
        if (r < 0.300f) return Feature::Grass;
        if (r < 0.307f) return Feature::Oak;
        if (r < 0.311f) return Feature::TallTree;
        if (r < 0.314f) return Feature::BushyTree;
        if (r < 0.317f) return Feature::FallenLog;
        return Feature::None;
    }

    static float roll(uint32_t seed, uint32_t salt) {
        return static_cast<float>((seed ^ salt) % 1000) / 1000.0f;
    }
};

class DesertBiome : public Biome {
public:
    DesertBiome() {
        name = "Desert";
        biomeId = 2;
        temperature = 1.0f;
        humidity = 0.1f;
        spawnWeight = 15.0f;
        surfaceBlock = BlockType::Sand;
        subSurfaceBlock = BlockType::Sand;
        stoneBlock = BlockType::Sand;
    }

    float getBaseHeight(int worldX, int worldZ, const siv::PerlinNoise* noise) const {
        float baseNoise = getHeightNoise(worldX, worldZ, noise, 0.02f, 2);
        return SEA_LEVEL + 2.0f + baseNoise * 3.0f;
    }

    int getSubSurfaceDepth() const override { return 5; }

    float getStructureSpawnChance() const override { return 0.05f; }

    void placeStructure(ChunkBlockSink& sink, int worldX, int surfaceY, int worldZ,
                        uint32_t seed, const TerrainSampler& terrain) const override {

        if (static_cast<float>((seed ^ 0x9A5Fu) % 1000) / 1000.0f >= 0.004f) {
            return;
        }
        structures::cactus(sink, worldX, surfaceY, worldZ, seed);
    }
};

class MountainBiome : public Biome {
public:
    MountainBiome() {
        name = "Mountain";
        biomeId = 3;
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

    void placeStructure(ChunkBlockSink& sink, int worldX, int surfaceY, int worldZ,
                        uint32_t seed, const TerrainSampler& terrain) const override {
        const float r = static_cast<float>((seed ^ 0x7C3Du) % 1000) / 1000.0f;
        if (r < 0.005f) {
            structures::boulder(sink, worldX, surfaceY, worldZ, seed, terrain);
        } else if (r < 0.008f) {
            structures::tree(sink, worldX, surfaceY, worldZ, seed, 6, 3, 1, 4);
        }
    }
};

class OceanBiome : public Biome {
public:
    OceanBiome() {
        name = "Ocean";
        biomeId = 4;
        temperature = 0.5f;
        humidity = 1.0f;
        spawnWeight = 5.0f; // todo chage after tests
        surfaceBlock = BlockType::Sand;
        subSurfaceBlock = BlockType::Sand;
        stoneBlock = BlockType::Stone;
    }

    float getBaseHeight(int worldX, int worldZ, const siv::PerlinNoise* noise) const {
        float baseNoise = getHeightNoise(worldX, worldZ, noise, 0.004f, 2);
        return SEA_LEVEL - 10.0f + baseNoise * 6.0f;
    }

    int getSubSurfaceDepth() const override { return 4; }

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
