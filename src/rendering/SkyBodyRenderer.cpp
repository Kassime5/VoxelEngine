//
// Created by maxim on 13/08/2026.
//

#include "SkyBodyRenderer.h"

#include "src/core/GL.h"
#include <iostream>

#include "Shader.h"
#include "ShaderManager.h"

namespace {
    // Only the ratio matters
    constexpr float BODY_DISTANCE = 100.0f;
    constexpr float SUN_RADIUS = 9.0f;
    constexpr float MOON_RADIUS = 7.0f;
    constexpr glm::vec3 MOON_TINT{0.85f, 0.88f, 1.00f};
}

SkyBodyRenderer::SkyBodyRenderer() : VAO(0), VBO(0), bodyShader(nullptr) {
    ShaderManager& sm = ShaderManager::getInstance();
    sm.addShader("skybody", "assets/shader/sky/body.vs.glsl",
                 "assets/shader/sky/body.fs.glsl");
    bodyShader = sm.getShader("skybody");

    if (!sunTexture.load("assets/textures/skybox/sun.png", false)) {
        std::cerr << "Failed to load sun texture!" << std::endl;
    }
    if (!moonTexture.load("assets/textures/skybox/moon.png", false)) {
        std::cerr << "Failed to load moon texture!" << std::endl;
    }

    for (Texture* texture : {&sunTexture, &moonTexture}) {
        texture->setFilterMode(Texture::FilterMode::Nearest, Texture::FilterMode::Nearest);
        texture->setWrapMode(Texture::WrapMode::ClampToEdge, Texture::WrapMode::ClampToEdge);
    }

    setupMesh();
}

SkyBodyRenderer::~SkyBodyRenderer() {
    glDeleteVertexArrays(1, &VAO);
    glDeleteBuffers(1, &VBO);
}

void SkyBodyRenderer::setupMesh() {
    // Two triangles of corner offsets. The vertex shader turns these into world positions
    // against whichever body's basis is current, so this never changes.
    constexpr float corners[] = {
        -1.0f, -1.0f,
         1.0f, -1.0f,
         1.0f,  1.0f,

        -1.0f, -1.0f,
         1.0f,  1.0f,
        -1.0f,  1.0f,
    };

    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);

    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(corners), corners, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    glBindVertexArray(0);
}

void SkyBodyRenderer::draw(const glm::mat4& view, const glm::mat4& projection, const SunState& sun) {
    // Saved and restored rather than assumed
    GLboolean cullFace;
    glGetBooleanv(GL_CULL_FACE, &cullFace);
    glDisable(GL_CULL_FACE);

    glDepthFunc(GL_LEQUAL);
    glDepthMask(GL_FALSE);

    bodyShader->use();
    bodyShader->setMat4("view", glm::mat4(glm::mat3(view)));
    bodyShader->setMat4("projection", projection);
    bodyShader->setInt("bodyTexture", 0);

    glBindVertexArray(VAO);

    drawBody(sun.direction, sunTexture, sun.color, SUN_RADIUS);
    // Opposite end of the same axis, so the moon is up exactly when the sun is not
    drawBody(-sun.direction, moonTexture, MOON_TINT, MOON_RADIUS);

    glBindVertexArray(0);

    glDepthMask(GL_TRUE);
    glDepthFunc(GL_LESS);
    if (cullFace) {
        glEnable(GL_CULL_FACE);
    }
}

void SkyBodyRenderer::drawBody(const glm::vec3& direction, const Texture& texture,
                               const glm::vec3& tint, float radius) const {
    const glm::vec3 forward = glm::normalize(direction);
    const glm::vec3 rightDir = glm::normalize(glm::cross(DayCycle::getOrbitAxis(), forward));
    const glm::vec3 upDir = glm::cross(forward, rightDir);

    bodyShader->setVec3("bodyCenter", forward * BODY_DISTANCE);
    bodyShader->setVec3("bodyRight", rightDir * radius);
    bodyShader->setVec3("bodyUp", upDir * radius);
    bodyShader->setVec3("tint", tint);

    texture.bind(0);
    glDrawArrays(GL_TRIANGLES, 0, 6);
}
