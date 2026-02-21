//
// Created by maxim on 15/02/2026.
//

#ifndef GLFWVOXEL_ANIMAL_H
#define GLFWVOXEL_ANIMAL_H

#include "Entity.h"
#include "src/game/ai/EntityAI.h"
#include "src/rendering/Meshes/ChunkMesh.h"
#include "src/rendering/Meshes/ModelManager.h"
#include "src/world/World.h"

class Animal : public Entity {
public:
    Animal(const glm::vec3& position, const std::string& animalType, World* world);

    void update(float deltaTime) override;
    void render(Shader& shader, glm::mat4 projection, glm::mat4 view) override;

private:
    EntityAI ai;
    std::string animalType;

    float walkAnimationTime = 0.0f;
    bool isWalking = false;
};

#endif //GLFWVOXEL_ANIMAL_H