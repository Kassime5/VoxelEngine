//
// Created by maxim on 25/08/2026.
//

#ifndef GLFWVOXEL_CLOUDRENDERER_H
#define GLFWVOXEL_CLOUDRENDERER_H

#include <glm/glm.hpp>

#include <vector>

#include "PerlinNoise/PerlinNoise.hpp"
#include "src/world/DayCycle.h"

class Shader;

struct CloudSettings {
    bool enabled = true;
    float height = 192.0f;
    // fraction of the grid filled
    float coverage = 0.45f;
    float opacity = 0.75f;
    // blocks per second along +X
    float windSpeed = 1.5f;
};

class CloudRenderer {
public:
    explicit CloudRenderer(siv::PerlinNoise::seed_type seed);
    ~CloudRenderer();

    CloudRenderer(const CloudRenderer&) = delete;
    CloudRenderer& operator=(const CloudRenderer&) = delete;

    CloudSettings& getSettings() { return settings; }
    const CloudSettings& getSettings() const { return settings; }

    void setGridSize(int renderDistance);
    void regenerate(siv::PerlinNoise::seed_type newSeed);
    void update(float deltaTime);
    void draw(const glm::mat4& projection, const glm::mat4& view, const SunState& sun, const glm::vec3& viewPos) const;

private:
    // Cells per tile side
    static constexpr float CELL_SIZE = 12.0f;
    static constexpr float CELL_HEIGHT = 6.0f;

    static constexpr int MIN_GRID = 32;
    static constexpr int MAX_GRID = 128;

    int gridSize = 64;
    float tileSpan = gridSize * CELL_SIZE;
    float fadeEnd = tileSpan * 0.8f;
    float fadeStart = tileSpan * 0.55f;

    static constexpr float NOISE_SCALE = 0.08f;
    static constexpr int NOISE_OCTAVES = 4;

    Shader* cloudShader = nullptr;
    unsigned int VAO = 0, VBO = 0, EBO = 0;
    int indexCount = 0;
    int vertexCount = 0;

    CloudSettings settings;
    siv::PerlinNoise::seed_type seed;
    float windOffset = 0.0f;
    float meshedCoverage = -1.0f;
    std::vector<float> field;

    void buildField();
    void buildMesh();
};

#endif //GLFWVOXEL_CLOUDRENDERER_H
