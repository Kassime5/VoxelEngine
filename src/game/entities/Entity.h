//
// Created by maxim on 14/02/2026.
//

#ifndef GLFWVOXEL_ENTITY_H
#define GLFWVOXEL_ENTITY_H

#include <glm/glm.hpp>
#include "../../rendering/Meshes/EntityMesh.h"
#include "src/rendering/Meshes/Model.h"
#include "src/utils/AABB.h"

class Shader;
class World;

enum class EntityType {
    Passive,
    Neutral,
    Hostile,
    Player
};

class Entity {
public:
    Entity(const glm::vec3& position, EntityType type, World* world);
    virtual ~Entity() = default;

    virtual void update(float deltaTime) = 0;
    virtual void render(Shader& shader, glm::mat4 projection, glm::mat4 view) = 0;

    void applyGravity(float deltaTime);
    void move(const glm::vec3& movement);
    bool isOnGround() const;

    const glm::vec3& getPosition() const { return position; }
    const glm::vec3& getVelocity() const { return velocity; }
    const glm::vec3& getRotation() const { return rotation; }
    const glm::ivec3& getChunkPosition() const { return chunkPosition; }

    World* getWorld() const { return world; }
    float getSpeed() const { return speed; }

    void setPosition(const glm::vec3& pos);
    void setVelocity(const glm::vec3& vel) { velocity = vel; }
    void setRotation(const glm::vec3& rot) { rotation = rot; }

    bool isAlive() const { return alive; }
    void kill() { alive = false; }

    EntityType getType() const { return entityType; }
    float getHealth() const { return health; }
    void damage(float amount) { health -= amount; if (health <= 0) kill(); }

    bool shouldJump(const glm::vec3& moveDirection) const;
    void jump();

    const AABB& getBoundingBox() const { return boundingBox; }
protected:
    glm::vec3 position;
    glm::vec3 velocity;
    glm::vec3 rotation;
    glm::ivec3 chunkPosition{};

    World* world;

    float health = 100.0f;
    float maxHealth = 100.0f;
    float speed = 4.0f;
    float jumpHeight = 2.0f;

    glm::vec3 size = glm::vec3(0.6f, 1.8f, 0.6f);
    AABB boundingBox;

    bool alive = true;
    bool onGround = false;
    EntityType entityType;

    void updateChunkPosition();
    void updateBoundingBox();
    bool checkCollision(const glm::vec3& pos) const;

    static constexpr float GRAVITY = 32.0f;
    static constexpr float JUMP_VELOCITY = 10.0f;

    // EntityMesh mesh;
    Model* entityModel = nullptr;
};


#endif //GLFWVOXEL_ENTITY_H