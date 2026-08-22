//
// Created by maxim on 13/08/2026.
//

#ifndef GLFWVOXEL_SKYBODYRENDERER_H
#define GLFWVOXEL_SKYBODYRENDERER_H

#include <glm/glm.hpp>

#include "Texture.h"
#include "src/world/DayCycle.h"

class Shader;

// Draws the sun and the moon as camera-facing textured quads just inside the far plane.
class SkyBodyRenderer {
public:
    SkyBodyRenderer();
    ~SkyBodyRenderer();

    SkyBodyRenderer(const SkyBodyRenderer&) = delete;
    SkyBodyRenderer& operator=(const SkyBodyRenderer&) = delete;

    void draw(const glm::mat4& view, const glm::mat4& projection, const SunState& sun);
private:
    unsigned int VAO, VBO;
    Shader* bodyShader;
    Texture sunTexture;
    Texture moonTexture;

    void setupMesh();
    void drawBody(const glm::vec3& direction, const Texture& texture,
                  const glm::vec3& tint, float radius) const;
};

#endif //GLFWVOXEL_SKYBODYRENDERER_H
