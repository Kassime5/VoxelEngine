//
// Created by maxim on 17/01/2026.
//

#ifndef GLFWVOXEL_SKYBOX_H
#define GLFWVOXEL_SKYBOX_H

#include <string>
#include <vector>
#include <glm/glm.hpp>

class Shader;

class Skybox {
public:
    Skybox();
    ~Skybox();

    bool load(const std::vector<std::string>& faces);
    void draw(const glm::mat4& view, const glm::mat4& projection);

private:
    unsigned int VAO, VBO;
    unsigned int textureID;

    void setupMesh();
    unsigned int loadCubemap(const std::vector<std::string>& faces);

    Shader* skyboxShader;
};

#endif //GLFWVOXEL_SKYBOX_H