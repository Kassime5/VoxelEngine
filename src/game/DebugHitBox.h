//
// Created by maxim on 20/02/2026.
//

#ifndef GLFWVOXEL_DEBUGHITBOX_H
#define GLFWVOXEL_DEBUGHITBOX_H

#include <glad/glad.h>
#include <glm/glm.hpp>
#include <vector>

#include "src/rendering/ShaderManager.h"
#include "src/utils/AABB.h"

class DebugHitbox {
public:
    DebugHitbox() {
        ShaderManager& sm = ShaderManager::getInstance();
        sm.addShader("highlight", "assets/shader/player/selection_box.vs.glsl",
                                   "assets/shader/player/selection_box.fs.glsl");
        shader = sm.getShader("highlight");

        setupMesh();
    }

    ~DebugHitbox() {
        glDeleteVertexArrays(1, &VAO);
        glDeleteBuffers(1, &VBO);
        glDeleteBuffers(1, &EBO);
    }

    void draw(const AABB& aabb, glm::vec3 rotation, glm::mat4 projection, glm::mat4 view, glm::vec3 color = glm::vec3(1.0f, 0.0f, 0.0f)) const {
        glm::vec3 boxSize = aabb.max - aabb.min;
        glm::vec3 center = (aabb.min + aabb.max) * 0.5f;

        glm::mat4 model = glm::mat4(1.0f);
        model = glm::translate(model, center);
        model = glm::rotate(model, rotation.y, glm::vec3(0, 1, 0));
        model = glm::scale(model, boxSize);
        model = glm::translate(model, glm::vec3(-0.5f, -0.5f, -0.5f));

        shader->use();
        shader->setMat4("model", model);
        shader->setMat4("viewProj", projection * view);
        shader->setVec3("lineColor", color);

        glLineWidth(1.5f);
        glDisable(GL_DEPTH_TEST);

        glBindVertexArray(VAO);
        glDrawElements(GL_LINES, 24, GL_UNSIGNED_INT, 0);
        glBindVertexArray(0);

        glEnable(GL_DEPTH_TEST);
        glLineWidth(1.0f);
    }

private:
    unsigned int VAO, VBO, EBO;
    Shader* shader = nullptr;

    void setupMesh() {
        float vertices[] = {
            0.0f, 0.0f, 0.0f,
            1.0f, 0.0f, 0.0f,
            1.0f, 1.0f, 0.0f,
            0.0f, 1.0f, 0.0f,
            0.0f, 0.0f, 1.0f,
            1.0f, 0.0f, 1.0f,
            1.0f, 1.0f, 1.0f,
            0.0f, 1.0f, 1.0f
        };

        unsigned int indices[] = {
            0, 1,  1, 2,  2, 3,  3, 0,
            4, 5,  5, 6,  6, 7,  7, 4,
            0, 4,  1, 5,  2, 6,  3, 7
        };

        glGenVertexArrays(1, &VAO);
        glGenBuffers(1, &VBO);
        glGenBuffers(1, &EBO);

        glBindVertexArray(VAO);

        glBindBuffer(GL_ARRAY_BUFFER, VBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(0);

        glBindVertexArray(0);
    }
};

#endif //GLFWVOXEL_DEBUGHITBOX_H