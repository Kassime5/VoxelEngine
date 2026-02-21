//
// Created by maxim on 15/02/2026.
//

#include "EntityAI.h"
#include "../entities/Entity.h"
#include "src/world/World.h"

void WanderState::enter(Entity* entity) {
    targetPosition = findRandomWalkablePosition(entity, entity->getWorld());
    nextWanderTime = 5.0f + static_cast<float>(rand() % 300) / 100.0f;
}

void WanderState::update(Entity* entity, float deltaTime, World* world) {
    glm::vec3 direction = targetPosition - entity->getPosition();
    direction.y = 0;

    float distanceToTarget = glm::length(direction);

    if (distanceToTarget <= 0.5f) {
        wanderTimer += deltaTime;
        if (wanderTimer >= nextWanderTime) {
            targetPosition = findRandomWalkablePosition(entity, world);
            wanderTimer = 0.0f;
            nextWanderTime = 5.0f + static_cast<float>(rand() % 300) / 100.0f;
        }
    } else {
        wanderTimer = 0.0f;

        direction = glm::normalize(direction);
        glm::vec3 movement = direction * entity->getSpeed() * deltaTime;
        entity->move(movement);

        float angle = std::atan2(direction.x, direction.z);
        entity->setRotation(glm::vec3(0, angle, 0));
    }
}

glm::vec3 WanderState::findRandomWalkablePosition(Entity* entity, World* world) {
    glm::vec3 current = entity->getPosition();

    for (int attempt = 0; attempt < 10; attempt++) {
        float angle = static_cast<float>(rand()) / RAND_MAX * 2.0f * 3.14159f;
        float distance = 5.0f + static_cast<float>(rand() % 700) / 100.0f;

        glm::vec3 candidate(
            current.x + std::cos(angle) * distance,
            current.y,
            current.z + std::sin(angle) * distance
        );

        // Check if position is walkable
        int x = static_cast<int>(candidate.x);
        int y = static_cast<int>(candidate.y);
        int z = static_cast<int>(candidate.z);

        // Find ground level
        for (int dy = 2; dy >= -2; dy--) {
            if (world->getBlock(x, y + dy, z) != BlockType::Air &&
                world->getBlock(x, y + dy + 1, z) == BlockType::Air) {
                candidate.y = y + dy + 1;
                return candidate;
            }
        }
    }

    return current;
}

void IdleState::enter(Entity* entity) {
    idleTimer = 0.0f;
    idleDuration = 1.0f + static_cast<float>(rand() % 400) / 100.0f;
    entity->setVelocity(glm::vec3(0));
}

void IdleState::update(Entity* entity, float deltaTime, World* world) {
    idleTimer += deltaTime;
}

bool IdleState::shouldTransition(Entity* entity, World* world) const {
    return idleTimer >= idleDuration;
}

std::shared_ptr<AIState> IdleState::getNextState() const {
    return std::make_shared<WanderState>();
}

EntityAI::EntityAI() {
    currentState = std::make_shared<IdleState>();
}

void EntityAI::update(Entity* entity, float deltaTime, World* world) {
    if (currentState) {
        currentState->update(entity, deltaTime, world);

        if (currentState->shouldTransition(entity, world)) {
            setState(currentState->getNextState(), entity);
        }
    }
}

void EntityAI::setState(std::shared_ptr<AIState> newState, Entity* entity) {
    if (currentState) {
        currentState->exit(entity);
    }
    currentState = newState;
    if (currentState) {
        currentState->enter(entity);
    }
}