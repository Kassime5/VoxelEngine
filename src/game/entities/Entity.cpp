//
// Created by maxim on 14/02/2026.
//

#include "Entity.h"
#include "src/world/World.h"

Entity::Entity(const glm::vec3& position, EntityType type, World* world)
    : position(position), velocity(0.0f), rotation(0.0f), entityType(type), world(world) {
    updateChunkPosition();
    // mesh = EntityMesh::createCube();
    boundingBox.min = position + glm::vec3(-0.3f, 0, -0.3f/2);
    boundingBox.max = position + glm::vec3(0.3f/2, 0.8f, 0.3f/2);
}

void Entity::setPosition(const glm::vec3& pos) {
    position = pos;
    updateChunkPosition();
}

void Entity::updateBoundingBox() {
    boundingBox.min = position - glm::vec3(size.x / 2, 0, size.z / 2);
    boundingBox.max = position + glm::vec3(size.x / 2, size.y, size.z / 2);
}

void Entity::updateChunkPosition() {
    chunkPosition = glm::ivec3(
        std::floor(position.x / Chunk::SIZE),
        0,
        std::floor(position.z / Chunk::SIZE)
    );
}

void Entity::applyGravity(float deltaTime) {
    velocity.y -= GRAVITY * deltaTime;

    if (velocity.y < -78.4f) {
        velocity.y = -78.4f;
    }
}

bool Entity::isOnGround() const {
    glm::vec3 feetPos = position;
    feetPos.y -= 0.1f;
    int x = static_cast<int>(std::floor(feetPos.x));
    int y = static_cast<int>(std::floor(feetPos.y));
    int z = static_cast<int>(std::floor(feetPos.z));

    BlockType below = world->getBlock(x, y, z);
    return below != BlockType::Air;
}

void Entity::move(const glm::vec3& movement) {
    glm::vec3 newPos = position + movement;

    if (!checkCollision(glm::vec3(newPos.x, position.y, position.z))) {
        position.x = newPos.x;
    }

    if (!checkCollision(glm::vec3(position.x, newPos.y, position.z))) {
        position.y = newPos.y;
    } else {
        velocity.y = 0;
    }

    if (!checkCollision(glm::vec3(position.x, position.y, newPos.z))) {
        position.z = newPos.z;
    }

    updateChunkPosition();
    updateBoundingBox();
}

bool Entity::checkCollision(const glm::vec3& pos) const {
    glm::vec3 min = pos - glm::vec3(size.x / 2, -0.01f, size.z / 2);
    glm::vec3 max = pos + glm::vec3(size.x / 2, size.y, size.z / 2);

    for (int x = static_cast<int>(std::floor(min.x)); x <= static_cast<int>(std::floor(max.x)); x++) {
        for (int y = static_cast<int>(std::floor(min.y)); y <= static_cast<int>(std::floor(max.y)); y++) {
            for (int z = static_cast<int>(std::floor(min.z)); z <= static_cast<int>(std::floor(max.z)); z++) {
                BlockType block = world->getBlock(x, y, z);
                if (!canGoThrough(block)) {
                    return true;
                }
            }
        }
    }
    return false;
}

// TODO
// bool Entity::shouldJump(const glm::vec3& moveDirection) const {
//     if (!isOnGround()) return false;
//
//     glm::vec3 checkDir = glm::normalize(glm::vec3(moveDirection.x, 0, moveDirection.z));
//     glm::vec3 frontPos = position + checkDir * (size.x / 2 + 0.3f);
//
//     int x = static_cast<int>(std::floor(frontPos.x));
//     int y = static_cast<int>(std::floor(position.y));
//     int z = static_cast<int>(std::floor(frontPos.z));
//
//     bool blockAtFeet = world->getBlock(x, y, z) != BlockType::Air;
//     bool airAbove = world->getBlock(x, y + 1, z) == BlockType::Air && world->getBlock(x, y + 2, z) == BlockType::Air;
//
//     return blockAtFeet && airAbove;
// }
//
// void Entity::jump() {
//     if (isOnGround()) {
//         velocity.y = JUMP_VELOCITY * jumpHeight;
//     }
// }
