//
// Created by maxim on 04/03/2026.
//

#ifndef GLFWVOXEL_HUDRENDERER_H
#define GLFWVOXEL_HUDRENDERER_H

#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include "src/rendering/ShaderManager.h"

class HUDRenderer {
public:
    HUDRenderer(int screenWidth, int screenHeight)
        : screenWidth(screenWidth), screenHeight(screenHeight) {

        ShaderManager& sm = ShaderManager::getInstance();
        sm.addShader("hud", "assets/shader/hud/hud.vs.glsl", "assets/shader/hud/hud.fs.glsl");
        hudShader = sm.getShader("hud");

        setupQuad();

        projection = glm::ortho(0.0f, static_cast<float>(screenWidth), 0.0f, static_cast<float>(screenHeight));
    }

    ~HUDRenderer() {
        glDeleteVertexArrays(1, &VAO);
        glDeleteBuffers(1, &VBO);
    }

    void drawRect(float x, float y, float width, float height, const glm::vec4& color = glm::vec4(1.0f)) const {
        hudShader->use();
        hudShader->setMat4("projection", projection);
        hudShader->setBool("useTexture", false);
        hudShader->setVec4("color", color);

        glm::mat4 model = glm::mat4(1.0f);
        // Translate so (x, y) is the center
        model = glm::translate(model, glm::vec3(x - width * 0.5f, y - height * 0.5f, 0.0f));
        model = glm::scale(model, glm::vec3(width, height, 1.0f));
        hudShader->setMat4("model", model);

        GLboolean depthTest, cullFace, depthMask;
        glGetBooleanv(GL_DEPTH_TEST, &depthTest);
        glGetBooleanv(GL_CULL_FACE, &cullFace);
        glGetBooleanv(GL_DEPTH_WRITEMASK, &depthMask);

        glDisable(GL_DEPTH_TEST);
        glDisable(GL_CULL_FACE);
        glDepthMask(GL_FALSE);

        glBindVertexArray(VAO);
        glBindVertexArray(VAO);
        glDrawArrays(GL_TRIANGLES, 0, 6);

        if (depthTest) glEnable(GL_DEPTH_TEST);
        if (cullFace)  glEnable(GL_CULL_FACE);
        glDepthMask(depthMask);
    }

    void drawTexturedRect(float x, float y, float width, float height, GLuint textureId, const glm::vec4& tint = glm::vec4(1.0f)) const {
        hudShader->use();
        hudShader->setMat4("projection", projection);
        hudShader->setBool("useTexture", true);
        hudShader->setVec4("color", tint);
        hudShader->setInt("hudTexture", 0);

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, textureId);

        glm::mat4 model = glm::mat4(1.0f);
        model = glm::translate(model, glm::vec3(x - width * 0.5f, y - height * 0.5f, 0.0f));
        model = glm::scale(model, glm::vec3(width, height, 1.0f));
        hudShader->setMat4("model", model);

        glDisable(GL_DEPTH_TEST);
        glDisable(GL_CULL_FACE);
        glDepthMask(GL_FALSE);

        glBindVertexArray(VAO);
        glDrawArrays(GL_TRIANGLES, 0, 6);
        glBindVertexArray(0);
        glEnable(GL_DEPTH_TEST);
        glEnable(GL_CULL_FACE);
        glDepthMask(GL_TRUE);
    }

    void drawCrosshair(float size = 10.0f, float thickness = 2.0f, const glm::vec4& color = glm::vec4(1.0f, 1.0f, 1.0f, 0.85f)) const {
        float cx = screenWidth * 0.5f;
        float cy = screenHeight * 0.5f;

        drawRect(cx, cy, size * 2.0f, thickness, color);
        drawRect(cx, cy, thickness, size * 2.0f, color);
    }

    void onResize(int newWidth, int newHeight) {
        screenWidth  = newWidth;
        screenHeight = newHeight;
        projection = glm::ortho(0.0f, static_cast<float>(screenWidth), 0.0f, static_cast<float>(screenHeight));
    }

private:
    int screenWidth, screenHeight;
    unsigned int VAO, VBO;
    Shader* hudShader = nullptr;
    glm::mat4 projection;

    void setupQuad() {
        float vertices[] = {
            0.0f, 1.0f, 0.0f, 1.0f,
            1.0f, 0.0f, 1.0f, 0.0f,
            0.0f, 0.0f, 0.0f, 0.0f,
            0.0f, 1.0f, 0.0f, 1.0f,
            1.0f, 1.0f, 1.0f, 1.0f,
            1.0f, 0.0f, 1.0f, 0.0f,
        };

        glGenVertexArrays(1, &VAO);
        glGenBuffers(1, &VBO);

        glBindVertexArray(VAO);
        glBindBuffer(GL_ARRAY_BUFFER, VBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

        // position (location 0)
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(0);

        // uv (location 1)
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
        glEnableVertexAttribArray(1);

        glBindVertexArray(0);
    }
};

#endif //GLFWVOXEL_HUDRENDERER_H