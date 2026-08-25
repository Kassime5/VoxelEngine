//
// Created by maxim on 25/08/2026.
//

#include "CloudRenderer.h"

#include "src/core/GL.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

#include "Frustum.h"
#include "Shader.h"
#include "ShaderManager.h"
#include "src/debug/RenderStats.h"

namespace {
    struct CloudVertex {
        glm::vec3 position;
        float shade;
    };

    constexpr float TOP_SHADE = 1.0f;
    constexpr float BOTTOM_SHADE = 0.72f;
    constexpr float SIDE_X_SHADE = 0.86f;
    constexpr float SIDE_Z_SHADE = 0.80f;

    // Cross-faded against its own wrapped copies
    float tileableNoise(const siv::PerlinNoise& noise, int x, int y, int grid, float scale, int octaves) {
        const auto at = [&](float fx, float fy) {
            return static_cast<float>(noise.octave2D_01(fx * scale, fy * scale, octaves));
        };

        const float span = static_cast<float>(grid);
        const float u = static_cast<float>(x) / span;
        const float v = static_cast<float>(y) / span;

        return at(x, y) * (1.0f - u) * (1.0f - v)+ at(x - span, y) * u * (1.0f - v)
                + at(x, y - span) * (1.0f - u) * v+ at(x - span, y - span) * u * v;
    }
}

CloudRenderer::CloudRenderer(siv::PerlinNoise::seed_type seed)
    : seed(seed) {
    ShaderManager& sm = ShaderManager::getInstance();
    sm.addShader("cloud", "assets/shader/cloud/cloud.vs.glsl",
                 "assets/shader/cloud/cloud.fs.glsl");
    cloudShader = sm.getShader("cloud");

    buildField();
    buildMesh();
}

CloudRenderer::~CloudRenderer() {
    glDeleteVertexArrays(1, &VAO);
    glDeleteBuffers(1, &VBO);
    glDeleteBuffers(1, &EBO);
}

void CloudRenderer::setGridSize(int renderDistance) {
    const int cells = std::clamp(renderDistance * 8, MIN_GRID, MAX_GRID);
    // The slider fires on every drag step, and each size change remeshes the layer.
    if (cells == gridSize) {
        return;
    }

    gridSize = cells;
    tileSpan = static_cast<float>(gridSize) * CELL_SIZE;
    fadeEnd = tileSpan * 0.8f;
    fadeStart = tileSpan * 0.55f;

    buildField();
    buildMesh();
}

void CloudRenderer::regenerate(siv::PerlinNoise::seed_type newSeed) {
    seed = newSeed;
    buildField();
    buildMesh();
}

void CloudRenderer::update(float deltaTime) {
    if (settings.coverage != meshedCoverage) {
        buildMesh();
    }

    windOffset += settings.windSpeed * deltaTime;
    // The layer is periodic, so wrapping keeps the offset small and precise.
    windOffset -= std::floor(windOffset / tileSpan) * tileSpan;
}

void CloudRenderer::buildField() {
    const siv::PerlinNoise noise{seed};

    field.resize(gridSize * gridSize);
    for (int j = 0; j < gridSize; ++j) {
        for (int i = 0; i < gridSize; ++i) {
            field[j * gridSize + i] = tileableNoise(noise, i, j, gridSize, NOISE_SCALE, NOISE_OCTAVES);
        }
    }
}

void CloudRenderer::buildMesh() {
    const float coverage = std::clamp(settings.coverage, 0.0f, 1.0f);
    std::vector<float> ranked(field);
    const std::size_t cut = static_cast<std::size_t>((1.0f - coverage) * static_cast<float>(ranked.size() - 1));
    std::nth_element(ranked.begin(), ranked.begin() + cut, ranked.end());
    const float threshold = ranked[cut];

    std::vector<std::uint8_t> filled(field.size());
    for (std::size_t i = 0; i < field.size(); ++i) {
        filled[i] = field[i] > threshold ? 1 : 0;
    }

    std::vector<CloudVertex> vertices;
    std::vector<unsigned int> indices;

    const auto solid = [&](int i, int j) {
        return filled[((j + gridSize) % gridSize) * gridSize + ((i + gridSize) % gridSize)] != 0;
    };

    const auto addFace = [&](const glm::vec3& a, const glm::vec3& b, const glm::vec3& c,
                             const glm::vec3& d, float shade) {
        const auto base = static_cast<unsigned int>(vertices.size());
        vertices.push_back({a, shade});
        vertices.push_back({b, shade});
        vertices.push_back({c, shade});
        vertices.push_back({d, shade});
        indices.insert(indices.end(), {base, base + 1, base + 2, base, base + 2, base + 3});
    };

    constexpr float y0 = 0.0f;
    constexpr float y1 = CELL_HEIGHT;

    for (int j = 0; j < gridSize; ++j) {
        for (int i = 0; i < gridSize; ++i) {
            if (!solid(i, j)) {
                continue;
            }

            const float x0 = static_cast<float>(i) * CELL_SIZE;
            const float x1 = x0 + CELL_SIZE;
            const float z0 = static_cast<float>(j) * CELL_SIZE;
            const float z1 = z0 + CELL_SIZE;

            addFace({x0, y1, z1}, {x1, y1, z1}, {x1, y1, z0}, {x0, y1, z0}, TOP_SHADE);
            addFace({x0, y0, z0}, {x1, y0, z0}, {x1, y0, z1}, {x0, y0, z1}, BOTTOM_SHADE);

            if (!solid(i + 1, j))
                addFace({x1, y0, z1}, {x1, y0, z0}, {x1, y1, z0}, {x1, y1, z1}, SIDE_X_SHADE);
            if (!solid(i - 1, j))
                addFace({x0, y0, z0}, {x0, y0, z1}, {x0, y1, z1}, {x0, y1, z0}, SIDE_X_SHADE);
            if (!solid(i, j + 1))
                addFace({x0, y0, z1}, {x1, y0, z1}, {x1, y1, z1}, {x0, y1, z1}, SIDE_Z_SHADE);
            if (!solid(i, j - 1))
                addFace({x1, y0, z0}, {x0, y0, z0}, {x0, y1, z0}, {x1, y1, z0}, SIDE_Z_SHADE);
        }
    }

    meshedCoverage = settings.coverage;
    indexCount = static_cast<int>(indices.size());
    vertexCount = static_cast<int>(vertices.size());

    if (VAO == 0) {
        glGenVertexArrays(1, &VAO);
        glGenBuffers(1, &VBO);
        glGenBuffers(1, &EBO);
    }

    glBindVertexArray(VAO);

    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER,
                 static_cast<GLsizeiptr>(vertices.size() * sizeof(CloudVertex)),
                 vertices.data(), GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                 static_cast<GLsizeiptr>(indices.size() * sizeof(unsigned int)),
                 indices.data(), GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(CloudVertex),
                          reinterpret_cast<void*>(offsetof(CloudVertex, position)));
    glEnableVertexAttribArray(0);

    glVertexAttribPointer(1, 1, GL_FLOAT, GL_FALSE, sizeof(CloudVertex),
                          reinterpret_cast<void*>(offsetof(CloudVertex, shade)));
    glEnableVertexAttribArray(1);

    glBindVertexArray(0);
}

void CloudRenderer::draw(const glm::mat4& projection, const glm::mat4& view,
                         const SunState& sun, const glm::vec3& viewPos) const {
    if (!settings.enabled || indexCount == 0 || cloudShader == nullptr) {
        return;
    }

    GLboolean cullFace;
    glGetBooleanv(GL_CULL_FACE, &cullFace);
    glEnable(GL_CULL_FACE);
    // Blended, so a near tile must not stop a farther one from drawing.
    glDepthMask(GL_FALSE);

    const glm::mat4 viewProj = projection * view;

    Frustum frustum;
    frustum.extractFromMatrix(viewProj);

    cloudShader->use();
    cloudShader->setMat4("viewProj", viewProj);
    cloudShader->setVec3("viewPos", viewPos);
    cloudShader->setVec3("lightColor", sun.color);
    cloudShader->setFloat("sunIntensity", sun.intensity);
    cloudShader->setFloat("opacity", settings.opacity);
    cloudShader->setFloat("fadeStart", fadeStart);
    cloudShader->setFloat("fadeEnd", fadeEnd);

    glBindVertexArray(VAO);

    // Origin of the tile the camera sits in, carried along by the wind.
    const float baseX =
        std::floor((viewPos.x - windOffset) / tileSpan) * tileSpan + windOffset;
    const float baseZ = std::floor(viewPos.z / tileSpan) * tileSpan;

    for (int tz = -1; tz <= 1; ++tz) {
        for (int tx = -1; tx <= 1; ++tx) {
            const glm::vec3 origin(baseX + static_cast<float>(tx) * tileSpan,
                                   settings.height,
                                   baseZ + static_cast<float>(tz) * tileSpan);
            const glm::vec3 corner = origin + glm::vec3(tileSpan, CELL_HEIGHT, tileSpan);

            const float dx = std::max({origin.x - viewPos.x, viewPos.x - corner.x, 0.0f});
            const float dz = std::max({origin.z - viewPos.z, viewPos.z - corner.z, 0.0f});
            if (dx * dx + dz * dz > fadeEnd * fadeEnd) {
                continue;
            }

            if (!frustum.isBoxInFrustum(origin, corner)) {
                continue;
            }

            cloudShader->setVec3("cloudOffset", origin);
            glDrawElements(GL_TRIANGLES, indexCount, GL_UNSIGNED_INT, nullptr);

            RenderStats::getInstance().addDrawCall();
            RenderStats::getInstance().addTriangles(indexCount / 3);
            RenderStats::getInstance().addVertices(vertexCount);
        }
    }

    glBindVertexArray(0);

    glDepthMask(GL_TRUE);
    if (!cullFace) {
        glDisable(GL_CULL_FACE);
    }
}
