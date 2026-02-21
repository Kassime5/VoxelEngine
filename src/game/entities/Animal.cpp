//
// Created by maxim on 15/02/2026.
//

#include "Animal.h"

Animal::Animal(const glm::vec3& position, const std::string& type, World* world)
    : Entity(position, EntityType::Passive, world), animalType(type) {

    // Different stats per animal type
    if (type == "sheep") {
        speed = 3.0f;
        maxHealth = 8.0f;
        size = glm::vec3(0.6f, 1.3f, 0.6f);
    } else if (type == "cow") {
        speed = 2.5f;
        maxHealth = 10.0f;
        size = glm::vec3(0.9f, 0.9f, 0.9f);
        entityModel = ModelManager::getInstance().getModel("cow");
    }

    health = maxHealth;
    updateBoundingBox();
}

void Animal::update(float deltaTime) {
    if (!alive) return;

    glm::vec3 oldPos = position;
    ai.update(this, deltaTime, world);

    applyGravity(deltaTime);

    velocity.x *= 0.95f;
    velocity.z *= 0.95f;

    if (glm::length(velocity) > 0.01f) {
        move(velocity * deltaTime);
    }

    isWalking = glm::length(position - oldPos) > 0.01f;
    if (isWalking) {
        walkAnimationTime += deltaTime * 5.0f;
    }
}

void Animal::render(Shader& shader, glm::mat4 projection, glm::mat4 view) {
    if (!alive) return;

    shader.use();

    glm::mat4 model = glm::mat4(1.0f);
    model = glm::translate(model, position);
    model = glm::rotate(model, rotation.y, glm::vec3(0, 1, 0));
    model = glm::rotate(model, glm::radians(180.0f), glm::vec3(0, 1, 0));

    shader.setMat4("model", model);
    shader.setMat4("projection", projection);
    shader.setMat4("view", view);

    entityModel->Draw(shader);
}
