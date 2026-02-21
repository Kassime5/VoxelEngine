//
// Created by maxim on 15/02/2026.
//

#ifndef GLFWVOXEL_ENTITYMANAGER_H
#define GLFWVOXEL_ENTITYMANAGER_H

#include <glm/glm.hpp>
#include <vector>
#include <memory>
#include <unordered_map>
#include "Entity.h"
#include "src/game/DebugHitBox.h"
#include "src/utils/VectorHash.h"

class EntityManager {
public:
    EntityManager();

    void update(float deltaTime, World* world);
    void render(glm::mat4 projection, glm::mat4 view);
    void renderDebug(glm::mat4 projection, glm::mat4 view);

    void addEntity(std::unique_ptr<Entity> entity);
    void removeDeadEntities();

    void spawnAnimalsInChunk(const glm::ivec3& chunkPos, World* world);

    std::vector<Entity*> getEntitiesInRadius(const glm::vec3& pos, float radius);
    int getEntityCount() const { return entities.size(); }

    void toggleHitboxes() { showHitboxes = !showHitboxes; }
private:
    DebugHitbox debugHitbox;
    bool showHitboxes = false;

    std::vector<std::unique_ptr<Entity>> entities;

    std::unordered_map<glm::ivec3, std::vector<Entity*>, IVec3Hash> spatialGrid;

    void updateSpatialGrid();
    // bool shouldSpawnInChunk(const glm::ivec3& chunkPos, const std::string& animalType);

    Shader* animalShader;
};


#endif //GLFWVOXEL_ENTITYMANAGER_H