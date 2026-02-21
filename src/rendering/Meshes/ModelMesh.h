//
// Created by maxim on 17/02/2026.
//

#ifndef GLFWVOXEL_MODELMESH_H
#define GLFWVOXEL_MODELMESH_H

#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "src/rendering/Shader.h"
#include <string>
#include <vector>

#define MAX_BONE_INFLUENCE 4

struct MeshVertex
{
    glm::vec3 Position;
    glm::vec3 Normal;
    glm::vec2 TexCoords;
    glm::vec3 Tangent;
    glm::vec3 Bitangent;
    int m_BoneIDs[MAX_BONE_INFLUENCE];
    float m_Weights[MAX_BONE_INFLUENCE];
};

struct MeshTexture
{
    unsigned int id;
    std::string type;
    std::string path;
};

class ModelMesh
{
public:
    std::vector<MeshVertex> vertices;
    std::vector<unsigned int> indices;
    std::vector<MeshTexture> textures;
    unsigned int VAO;

    ModelMesh(const std::vector<MeshVertex>& vertices, const std::vector<unsigned int>& indices,
        const std::vector<MeshTexture>& textures);

    void Draw(const Shader& shader);

private:
    unsigned int VBO, EBO;

    void setupMesh();
};


#endif //GLFWVOXEL_MODELMESH_H
