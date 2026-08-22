//
// Created by maxim on 09/02/2026.
//

#ifndef GLFWVOXEL_HIGHLIGHTBOX_H
#define GLFWVOXEL_HIGHLIGHTBOX_H

#include "src/core/GL.h"
#include <glm/glm.hpp>

#include "src/rendering/ShaderManager.h"
#include "src/world/World.h"

class HighlightBox {
public:
    HighlightBox() {
        ShaderManager& sm = ShaderManager::getInstance();
        sm.addShader("highlight", "assets/shader/player/selection_box.vs.glsl",
                                    "assets/shader/player/selection_box.fs.glsl");
        highlightBoxShader = sm.getShader("highlight");

        setupMesh();
    }

    ~HighlightBox() {
        glDeleteVertexArrays(1, &VAO);
        glDeleteBuffers(1, &VBO);
        glDeleteBuffers(1, &EBO);
    }

    void draw(RaycastResult highlightedBlock, glm::mat4 projection, glm::mat4 view) const {
        glLineWidth(2.0f);
        glDisable(GL_DEPTH_TEST);

        highlightBoxShader->use();

        glm::mat4 model = glm::mat4(1.0f);
        model = glm::translate(model, glm::vec3(highlightedBlock.hitPos));

        glm::mat4 viewProj = projection * view;

        highlightBoxShader->setMat4("model", model);
        highlightBoxShader->setMat4("viewProj", viewProj);
        highlightBoxShader->setVec3("lineColor", glm::vec3(0.0f, 0.0f, 0.0f));

        glBindVertexArray(VAO);
        glDrawElements(GL_LINES, 24, GL_UNSIGNED_INT, 0);
        glBindVertexArray(0);

        glEnable(GL_DEPTH_TEST);
        glLineWidth(1.0f);
    }

private:
    unsigned int VAO, VBO, EBO;

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

    Shader* highlightBoxShader = nullptr;
};


#endif //GLFWVOXEL_HIGHLIGHTBOX_H