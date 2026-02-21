//
// Created by maxim on 15/02/2026.
//

#ifndef GLFWVOXEL_ENTITYAI_H
#define GLFWVOXEL_ENTITYAI_H

#include <memory>
#include <vector>
#include <glm/glm.hpp>

class Entity;
class World;

class AIState {
public:
    virtual ~AIState() = default;
    virtual void enter(Entity* entity) {}
    virtual void exit(Entity* entity) {}
    virtual void update(Entity* entity, float deltaTime, World* world) = 0;
    virtual bool shouldTransition(Entity* entity, World* world) const { return false; }
    virtual std::shared_ptr<AIState> getNextState() const { return nullptr; }
};

// TODO: Move to another spot
class WanderState : public AIState {
public:
    void enter(Entity* entity) override;
    void update(Entity* entity, float deltaTime, World* world) override;

private:
    glm::vec3 targetPosition;
    float wanderTimer = 0.0f;
    float nextWanderTime = 0.0f;

    glm::vec3 findRandomWalkablePosition(Entity* entity, World* world);
};

class IdleState : public AIState {
public:
    void enter(Entity* entity) override;
    void update(Entity* entity, float deltaTime, World* world) override;
    bool shouldTransition(Entity* entity, World* world) const override;
    std::shared_ptr<AIState> getNextState() const override;

private:
    float idleTimer = 0.0f;
    float idleDuration = 3.0f;
};

class FleeState : public AIState {
public:
    FleeState(const glm::vec3& threatPos) : threatPosition(threatPos) {}

    void update(Entity* entity, float deltaTime, World* world) override;
    bool shouldTransition(Entity* entity, World* world) const override;
    std::shared_ptr<AIState> getNextState() const override;

private:
    glm::vec3 threatPosition;
    float safeDistance = 10.0f;
};

class EntityAI {
public:
    EntityAI();

    void update(Entity* entity, float deltaTime, World* world);
    void setState(std::shared_ptr<AIState> newState, Entity* entity);

    // Pathfinding helpers
    std::vector<glm::ivec3> findPath(const glm::vec3& start, const glm::vec3& end, World* world);

private:
    std::shared_ptr<AIState> currentState;
};


#endif //GLFWVOXEL_ENTITYAI_H