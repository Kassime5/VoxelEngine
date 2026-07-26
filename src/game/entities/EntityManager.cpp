//
// Created by maxim on 15/02/2026.
//

#include "EntityManager.h"
#include <iostream>
#include "Animal.h"
#include "src/world/Biome.h"
#include "src/world/Chunk.h"
#include "src/world/World.h"

EntityManager::EntityManager()
{
    ShaderManager& sm = ShaderManager::getInstance();
    // sm.addShader("animal", "assets/shader/entity/animal.vs.glsl",
    //                             "assets/shader/entity/animal.fs.glsl");
    sm.addShader("animal", "assets/shader/model/model.vs.glsl",
        "assets/shader/model/model.fs.glsl");
    animalShader = sm.getShader("animal");

    ModelManager& mm = ModelManager::getInstance();
    mm.addModel("cow", "assets/model/cow/Cow.obj");

    entities.reserve(100);
}

void EntityManager::update(float deltaTime, World* world) {
    for (auto& entity : entities) {
        entity->update(deltaTime);
    }

    removeDeadEntities();
    updateSpatialGrid();
}

void EntityManager::render(glm::mat4 projection, glm::mat4 view) {

    for (const auto& entity : entities) {
        entity->render(*animalShader, projection, view);
    }
}

void EntityManager::renderDebug(glm::mat4 projection, glm::mat4 view) {
    if (!showHitboxes) return;
    for (const auto& entity : entities) {
        debugHitbox.draw(entity->getBoundingBox(), entity->getRotation(), projection, view);
    }
}

void EntityManager::addEntity(std::unique_ptr<Entity> entity) {
    entities.push_back(std::move(entity));
}

void EntityManager::removeDeadEntities() {
    entities.erase(
        std::remove_if(entities.begin(), entities.end(),
            [](const std::unique_ptr<Entity>& e) { return !e->isAlive(); }),
        entities.end()
    );
}

void EntityManager::spawnAnimalsInChunk(const glm::ivec3& chunkPos, World* world) {
    if (entities.size() > 20)
        return;
    const Biome* biome = world->getCurrentPlayerBiome(
        chunkPos.x * Chunk::SIZE,
        chunkPos.z * Chunk::SIZE
    );

    // TODO: Change spawns in biomes
    std::vector<std::string> possibleAnimals;
    // if (biome->getName() == "Forest") {
    //     possibleAnimals = {"sheep", "pig", "cow"};
    // } else if (biome->getName() == "Desert") {
    //     possibleAnimals = {};
    // }
    // possibleAnimals = {"sheep", "pig", "cow"};
    possibleAnimals = {"cow"};

    if (possibleAnimals.empty()) return;

    Chunk* chunk = world->getChunk(chunkPos);

    // TODO: change random spawn
    int spawnCount = 1;//rand() % 4;

    for (int i = 0; i < spawnCount; i++) {
        std::string animalType = possibleAnimals[rand() % possibleAnimals.size()];

        int localX = 8 + rand() % 48;  // Avoid chunk edges
        int localZ = 8 + rand() % 48;

        int worldX = chunkPos.x * Chunk::SIZE + localX;
        int worldZ = chunkPos.z * Chunk::SIZE + localZ;
        int terrainHeight = chunk->getTerrainHeight(localX, localZ);

        // TODO: condition on spawn ? e.g. grass only
        glm::vec3 spawnPos(worldX + 0.5f, 300.0f, worldZ + 0.5f);
        addEntity(std::make_unique<Animal>(spawnPos, animalType, world));
    }
}

std::vector<Entity*> EntityManager::getEntitiesInChunk(const glm::ivec3& chunkPos) const {
    std::vector<Entity*> result;

    constexpr float chunkSize = static_cast<float>(Chunk::SIZE);
    float minX = static_cast<float>(chunkPos.x) * chunkSize;
    float minZ = static_cast<float>(chunkPos.z) * chunkSize;
    float maxX = minX + chunkSize;
    float maxZ = minZ + chunkSize;

    for (auto& entity : entities) {
        if (!entity) continue;
        glm::vec3 pos = entity->getPosition();
        if (pos.x >= minX && pos.x < maxX && pos.z >= minZ && pos.z < maxZ) {
            result.push_back(entity.get());
        }
    }

    return result;
}

void EntityManager::updateSpatialGrid() {
    spatialGrid.clear();
    for (auto& entity : entities) {
        spatialGrid[entity->getChunkPosition()].push_back(entity.get());
    }
}

std::vector<Entity*> EntityManager::getEntitiesInRadius(const glm::vec3& pos, float radius) {
    std::vector<Entity*> result;
    float radiusSq = radius * radius;

    for (auto& entity : entities) {
        float distSq = glm::distance(entity->getPosition(), pos);
        if (distSq <= radiusSq) {
            result.push_back(entity.get());
        }
    }

    return result;
}