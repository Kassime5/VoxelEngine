//
// Created by maxim on 17/01/2026.
//

#ifndef GLFWVOXEL_SKYBOX_H
#define GLFWVOXEL_SKYBOX_H

#include <glad/glad.h>
#include <string>
#include <vector>
#include "Shader.h"
#include <glm/glm.hpp>

class Skybox {
public:
    Skybox();
    ~Skybox();

    bool load(const std::vector<std::string>& faces);
    void draw(const glm::mat4& view, const glm::mat4& projection);

private:
    unsigned int m_VAO, m_VBO;
    unsigned int m_textureID;
    Shader m_shader;

    void setupMesh();
    unsigned int loadCubemap(const std::vector<std::string>& faces);
};


#endif //GLFWVOXEL_SKYBOX_H