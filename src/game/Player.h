//
// Created by maxim on 27/01/2026.
//

#ifndef GLFWVOXEL_PLAYER_H
#define GLFWVOXEL_PLAYER_H


#include <glm/glm.hpp>
#include "src/rendering/Camera.h"
#include "src/world/World.h"
#include "src/world/Block.h"

struct AABB {
    glm::vec3 min;
    glm::vec3 max;

    bool intersects(const AABB& other) const {
        return (min.x <= other.max.x && max.x >= other.min.x) &&
               (min.y <= other.max.y && max.y >= other.min.y) &&
               (min.z <= other.max.z && max.z >= other.min.z);
    }
};

class Player {
public:
    Player(const glm::vec3& startPosition);

    void update(float deltaTime, World* world);
    void processInput(Camera_Movement direction, bool sprinting, float deltaTime);
    void applyFriction(float deltaTime);
    void processVerticalInput(bool ascending, bool descending, float deltaTime);

    Camera& getCamera() { return camera; }
    const Camera& getCamera() const { return camera; }

    glm::vec3 getPosition() const { return position; }
    glm::vec3 getVelocity() const { return velocity; }
    bool isOnGround() const { return onGround; }

    bool isFlying() const { return flying; }
    void activateFlying();
    void deactivateFlying();

    void jump();

private:
    glm::vec3 position;
    glm::vec3 velocity;
    AABB boundingBox;
    bool flying = false;

    bool onGround;

    // Player properties
    static constexpr float WIDTH = 0.6f;
    static constexpr float HEIGHT = 1.8f;
    static constexpr float EYE_HEIGHT = 1.62f;

    static constexpr float GRAVITY = 32.0f;
    static constexpr float JUMP_VELOCITY = 10.0f;
    static constexpr float MOVE_SPEED = 7.0f;
    static constexpr float SPRINT_MULTIPLIER = 2.0f;

    Camera camera;

    // Collision
    void applyGravity(float deltaTime);
    void resolveCollisionAxis(float& delta, int axis, World* world);
    void updateBoundingBox();
    void updateCamera();

    bool isBlockSolid(BlockType type) const;
};


#endif //GLFWVOXEL_PLAYER_H