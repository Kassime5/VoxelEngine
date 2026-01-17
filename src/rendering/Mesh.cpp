//
// Created by maxim on 03/01/2026.
//


#include "Mesh.h"
#include "src/debug/RenderStats.h"

Mesh::Mesh() : VAO(0), VBO(0), EBO(0), initialized(false) {}

Mesh::Mesh(const std::vector<Vertex>& vertices, const std::vector<unsigned int>& indices)
    : vertices(vertices), indices(indices), VAO(0), VBO(0), EBO(0), initialized(false) {
    setupMesh();
}

Mesh::~Mesh() {
    cleanup();
}

Mesh::Mesh(Mesh&& other) noexcept
    : vertices(std::move(other.vertices)),
      indices(std::move(other.indices)),
      VAO(other.VAO),
      VBO(other.VBO),
      EBO(other.EBO),
      initialized(other.initialized) {
    other.VAO = 0;
    other.VBO = 0;
    other.EBO = 0;
    other.initialized = false;
}

Mesh& Mesh::operator=(Mesh&& other) noexcept {
    if (this != &other) {
        cleanup();

        vertices = std::move(other.vertices);
        indices = std::move(other.indices);
        VAO = other.VAO;
        VBO = other.VBO;
        EBO = other.EBO;
        initialized = other.initialized;

        other.VAO = 0;
        other.VBO = 0;
        other.EBO = 0;
        other.initialized = false;
    }
    return *this;
}

void Mesh::setData(const std::vector<Vertex>& newVertices, const std::vector<unsigned int>& newIndices) {
    vertices = newVertices;
    indices = newIndices;
    setupMesh();
}

void Mesh::clear() {
    cleanup();
    vertices.clear();
    indices.clear();
}

void Mesh::setupMesh() {
    if (vertices.empty()) return;

    // Clean up old buffers if they exist
    if (initialized) {
        cleanup();
    }

    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glGenBuffers(1, &EBO);

    glBindVertexArray(VAO);

    // Load vertex data
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(Vertex), vertices.data(), GL_STATIC_DRAW);

    // Load index data
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), indices.data(), GL_STATIC_DRAW);

    // Position attribute
    glVertexAttribPointer(0, 3, GL_UNSIGNED_BYTE, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, position));
    glEnableVertexAttribArray(0);

    // Tile index
    glVertexAttribIPointer(1, 1, GL_UNSIGNED_BYTE, sizeof(Vertex), (void*)offsetof(Vertex, tileIndex));
    glEnableVertexAttribArray(1);

    // Corner index
    glVertexAttribIPointer(2, 1, GL_UNSIGNED_BYTE, sizeof(Vertex), (void*)offsetof(Vertex, cornerIndex));
    glEnableVertexAttribArray(2);

    // NormalId attribute
    glVertexAttribIPointer(3, 1, GL_UNSIGNED_BYTE, sizeof(Vertex), (void*)offsetof(Vertex, normalId));
    glEnableVertexAttribArray(3);

    // Quad Width (NEW)
    glVertexAttribIPointer(4, 1, GL_UNSIGNED_BYTE, sizeof(Vertex), (void*)offsetof(Vertex, quadWidth));
    glEnableVertexAttribArray(4);

    // Quad Height (NEW)
    glVertexAttribIPointer(5, 1, GL_UNSIGNED_BYTE, sizeof(Vertex), (void*)offsetof(Vertex, quadHeight));
    glEnableVertexAttribArray(5);

    glBindVertexArray(0);
    initialized = true;
}

void Mesh::draw() const {
    if (!initialized || vertices.empty()) return;

    glBindVertexArray(VAO);
    glDrawElements(GL_TRIANGLES, indices.size(), GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);

    // Track rendering stats
    RenderStats::getInstance().addDrawCall();
    RenderStats::getInstance().addTriangles(indices.size() / 3);
    RenderStats::getInstance().addVertices(vertices.size());
}

void Mesh::cleanup() {
    if (initialized) {
        glDeleteVertexArrays(1, &VAO);
        glDeleteBuffers(1, &VBO);
        glDeleteBuffers(1, &EBO);
        VAO = VBO = EBO = 0;
        initialized = false;
    }
}

int Mesh::getVertexCount() const {
    return vertices.size();
}

int Mesh::getTriangleCount() const {
    return indices.size() / 3;
}
