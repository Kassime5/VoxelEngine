//
// Created by maxim on 04/03/2026.
//

#ifndef GLFWVOXEL_HUDRENDERER_H
#define GLFWVOXEL_HUDRENDERER_H

#include "src/core/GL.h"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include "src/rendering/ShaderManager.h"

#include <algorithm>

class HUDRenderer {
public:
    // Whole UI texture, for callers that do not slice an atlas
    static constexpr glm::vec4 FULL_UV{0.0f, 0.0f, 1.0f, 1.0f};

    HUDRenderer(int screenWidth, int screenHeight)
        : screenWidth(screenWidth), screenHeight(screenHeight) {

        ShaderManager& sm = ShaderManager::getInstance();
        sm.addShader("hud", "assets/shader/hud/hud.vs.glsl", "assets/shader/hud/hud.fs.glsl");
        hudShader = sm.getShader("hud");

        sm.addShader("hud_tile", "assets/shader/hud/hud.vs.glsl", "assets/shader/hud/hud_tile.fs.glsl");
        tileShader = sm.getShader("hud_tile");

        setupQuad();

        projection = glm::ortho(0.0f, static_cast<float>(screenWidth), 0.0f, static_cast<float>(screenHeight));
    }

    ~HUDRenderer() {
        glDeleteVertexArrays(1, &VAO);
        glDeleteBuffers(1, &VBO);
    }

    void drawRect(float x, float y, float width, float height, const glm::vec4& color = glm::vec4(1.0f)) const {
        hudShader->use();
        hudShader->setBool("useTexture", false);
        hudShader->setVec4("color", color);

        const SavedState saved = beginHud();
        submitQuad(*hudShader, x, y, width, height, FULL_UV);
        endHud(saved);
    }

    void drawTexturedRect(float x, float y, float width, float height, GLuint textureId,
                          const glm::vec4& tint = glm::vec4(1.0f)) const {
        drawSprite(x, y, width, height, textureId, FULL_UV, tint);
    }

    // uvRect is (minU, minV, maxU, maxV) into textureId -- lets one sheet hold many sprites
    void drawSprite(float x, float y, float width, float height, GLuint textureId,
                    const glm::vec4& uvRect, const glm::vec4& tint = glm::vec4(1.0f)) const {
        hudShader->use();
        hudShader->setBool("useTexture", true);
        hudShader->setVec4("color", tint);
        hudShader->setInt("hudTexture", 0);

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, textureId);

        const SavedState saved = beginHud();
        submitQuad(*hudShader, x, y, width, height, uvRect);
        endHud(saved);
    }

    // One layer of a GL_TEXTURE_2D_ARRAY, which is how the terrain atlas is stored
    void drawTile(float x, float y, float width, float height, GLuint arrayTextureId, int layer,
                  const glm::vec4& tint = glm::vec4(1.0f)) const {
        tileShader->use();
        tileShader->setVec4("color", tint);
        tileShader->setInt("tileTexture", 0);
        tileShader->setInt("tileLayer", layer);

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D_ARRAY, arrayTextureId);

        const SavedState saved = beginHud();
        submitQuad(*tileShader, x, y, width, height, FULL_UV);
        endHud(saved);

        glBindTexture(GL_TEXTURE_2D_ARRAY, 0);
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

    int getScreenWidth() const { return screenWidth; }
    int getScreenHeight() const { return screenHeight; }

    // Whole numbers only: pixel art scaled by a fraction shimmers on its 1px borders
    int getUIScale() const { return std::clamp(screenHeight / 360, 1, 6); }

private:
    struct SavedState {
        GLboolean depthTest;
        GLboolean cullFace;
        GLboolean depthMask;
    };

    int screenWidth, screenHeight;
    unsigned int VAO, VBO;
    Shader* hudShader = nullptr;
    Shader* tileShader = nullptr;
    glm::mat4 projection;

    SavedState beginHud() const {
        SavedState saved{};
        glGetBooleanv(GL_DEPTH_TEST, &saved.depthTest);
        glGetBooleanv(GL_CULL_FACE, &saved.cullFace);
        glGetBooleanv(GL_DEPTH_WRITEMASK, &saved.depthMask);

        glDisable(GL_DEPTH_TEST);
        glDisable(GL_CULL_FACE);
        glDepthMask(GL_FALSE);
        return saved;
    }

    void endHud(const SavedState& saved) const {
        if (saved.depthTest) glEnable(GL_DEPTH_TEST);
        if (saved.cullFace)  glEnable(GL_CULL_FACE);
        glDepthMask(saved.depthMask);
    }

    // (x, y) is the centre of the quad
    void submitQuad(const Shader& shader, float x, float y, float width, float height,
                    const glm::vec4& uvRect) const {
        shader.setMat4("projection", projection);
        shader.setVec4("uvRect", uvRect);

        glm::mat4 model = glm::mat4(1.0f);
        model = glm::translate(model, glm::vec3(x - width * 0.5f, y - height * 0.5f, 0.0f));
        model = glm::scale(model, glm::vec3(width, height, 1.0f));
        shader.setMat4("model", model);

        glBindVertexArray(VAO);
        glDrawArrays(GL_TRIANGLES, 0, 6);
        glBindVertexArray(0);
    }

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