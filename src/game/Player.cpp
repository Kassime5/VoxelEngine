//
// Created by maxim on 27/01/2026.
//

#include "Player.h"
#include <cmath>

Player::Player(const glm::vec3& startPosition)
    : position(startPosition),
      velocity(0.0f),
      onGround(false),
      camera(startPosition + glm::vec3(0, EYE_HEIGHT, 0)) {
    updateBoundingBox();
}

void Player::update(float deltaTime, World* world) {
    if (!flying) {
        applyGravity(deltaTime);
    }

    onGround = false;

    float dx = velocity.x * deltaTime;
    float dy = velocity.y * deltaTime;
    float dz = velocity.z * deltaTime;

    resolveCollisionAxis(dx, 0, world);
    resolveCollisionAxis(dy, 1, world);
    resolveCollisionAxis(dz, 2, world);

    applyFriction(deltaTime);
    updateBoundingBox();
    updateCamera();
}

void Player::processMovement(bool forward, bool backward, bool left, bool right, bool sprinting, float deltaTime) {
    glm::vec3 front, rightVec;

    if (flying) {
        front = camera.Front;
        rightVec = camera.Right;
    } else {
        front = glm::normalize(glm::vec3(camera.Front.x, 0.0f, camera.Front.z));
        rightVec = camera.Right;
    }

    float speed = MOVE_SPEED * (sprinting ? SPRINT_MULTIPLIER : 1.0f);
    if (flying) {
        speed *= 2.0f;
    }

    glm::vec3 moveDir(0.0f);

    if (forward)
        moveDir += front;
    if (backward)
        moveDir -= front;
    if (left)
        moveDir -= rightVec;
    if (right)
        moveDir += rightVec;

    // Normalize diagonal movement so you don't move faster diagonally
    if (glm::length(moveDir) > 0.0f) {
        moveDir = glm::normalize(moveDir);
    }

    if (flying) {
        velocity = moveDir * speed;
    } else {
        velocity.x = moveDir.x * speed;
        velocity.z = moveDir.z * speed;
    }
}

void Player::applyFriction(float deltaTime) {
    float friction = onGround ? 20.0f : 10.0f;
    if (flying)
        friction = 2000.0f;

    // Decelerate horizontal velocity
    float currentSpeed = std::sqrt(velocity.x * velocity.x + velocity.z * velocity.z);

    if (currentSpeed > 0.0f) {
        float newSpeed = currentSpeed - friction * deltaTime;
        if (newSpeed < 0.0f) newSpeed = 0.0f;

        float scale = newSpeed / currentSpeed;
        velocity.x *= scale;
        velocity.z *= scale;
    }
}

void Player::processVerticalInput(bool ascending, bool descending, float deltaTime) {
    if (!flying)
        return;

    float verticalSpeed = MOVE_SPEED * 2.0f;

    if (ascending) {
        velocity.y = verticalSpeed;
    } else if (descending) {
        velocity.y = -verticalSpeed;
    } else if (!ascending && !descending) {
        // If no vertical input, stop vertical movement
        velocity.y = 0.0f;
    }
}

void Player::jump() {
    if (onGround || flying) {
        velocity.y = JUMP_VELOCITY;
        onGround = false;
    }
}

void Player::applyGravity(float deltaTime) {
    velocity.y -= GRAVITY * deltaTime;

    // Terminal velocity
    if (velocity.y < -78.4f) {
        velocity.y = -78.4f;
    }
}

void Player::activateFlying() {
    velocity.y = 0.0f;
    flying = true;
}

void Player::deactivateFlying() {
    velocity.y = 0.0f;
    flying = false;
}

void Player::resolveCollisionAxis(float& delta, int axis, World* world) {
    if (delta == 0.0f) return;

    if (flying) {
        if (axis == 0) position.x += delta;
        else if (axis == 1) position.y += delta;
        else position.z += delta;
        return;
    }

    AABB newBox = boundingBox;
    if (axis == 0) {
        newBox.min.x += delta;
        newBox.max.x += delta;
    } else if (axis == 1) {
        newBox.min.y += delta;
        newBox.max.y += delta;
    } else {
        newBox.min.z += delta;
        newBox.max.z += delta;
    }

    int minX = static_cast<int>(std::floor(newBox.min.x));
    int minY = static_cast<int>(std::floor(newBox.min.y));
    int minZ = static_cast<int>(std::floor(newBox.min.z));
    int maxX = static_cast<int>(std::floor(newBox.max.x));
    int maxY = static_cast<int>(std::floor(newBox.max.y));
    int maxZ = static_cast<int>(std::floor(newBox.max.z));

    for (int x = minX; x <= maxX; x++) {
        for (int y = minY; y <= maxY; y++) {
            for (int z = minZ; z <= maxZ; z++) {
                BlockType block = world->getBlock(x, y, z);

                if (isBlockSolid(block)) {
                    AABB blockBox{
                        glm::vec3(x, y, z),
                        glm::vec3(x + 1, y + 1, z + 1)
                    };

                    if (newBox.intersects(blockBox)) {
                        if (axis == 1) {
                            if (delta < 0) {
                                onGround = true;
                            }
                            velocity.y = 0.0f;
                        }
                        delta = 0.0f;
                        return;
                    }
                }
            }
        }
    }

    if (axis == 0) position.x += delta;
    else if (axis == 1) position.y += delta;
    else position.z += delta;
}

void Player::updateBoundingBox() {
    boundingBox.min = position + glm::vec3(-WIDTH/2, 0, -WIDTH/2);
    boundingBox.max = position + glm::vec3(WIDTH/2, HEIGHT, WIDTH/2);
}

void Player::updateCamera() {
    camera.Position = position + glm::vec3(0, EYE_HEIGHT, 0);
}

bool Player::isBlockSolid(BlockType type) const {
    return type != BlockType::Air &&
           type != BlockType::TallGrass;
}