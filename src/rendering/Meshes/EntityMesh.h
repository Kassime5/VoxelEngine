//
// Created by maxim on 15/02/2026.
//

#ifndef GLFWVOXEL_ENTITYMESH_H
#define GLFWVOXEL_ENTITYMESH_H

#include "src/core/GL.h"
#include <vector>
#include <glm/glm.hpp>

struct SimpleVertex {
    glm::vec3 position;
    glm::vec3 normal;
};

class EntityMesh {
public:
    EntityMesh();
    ~EntityMesh();

    EntityMesh(const EntityMesh&) = delete;
    EntityMesh& operator=(const EntityMesh&) = delete;

    EntityMesh(EntityMesh&& other) noexcept;
    EntityMesh& operator=(EntityMesh&& other) noexcept;

    void setData(const std::vector<SimpleVertex>& vertices,
                 const std::vector<unsigned int>& indices);
    void draw() const;
    bool isEmpty() const { return vertexCount == 0; }

    static EntityMesh createCube();

private:
    unsigned int VAO, VBO, EBO;
    size_t vertexCount;
    size_t indexCount;
    bool initialized;

    void setupMesh(const std::vector<SimpleVertex>& vertices,
                   const std::vector<unsigned int>& indices);
    void cleanup();
};

#endif //GLFWVOXEL_ENTITYMESH_H