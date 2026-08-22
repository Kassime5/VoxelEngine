//
// Created by maxim on 17/01/2026.
//

#include "Skybox.h"
#include "src/core/GL.h"
#include "../stb_image.h"
#include <iostream>

#include "ShaderManager.h"

static const float skyboxVertices[] = {
    // positions
    -1.0f,  1.0f, -1.0f,
    -1.0f, -1.0f, -1.0f,
     1.0f, -1.0f, -1.0f,
     1.0f, -1.0f, -1.0f,
     1.0f,  1.0f, -1.0f,
    -1.0f,  1.0f, -1.0f,

    -1.0f, -1.0f,  1.0f,
    -1.0f, -1.0f, -1.0f,
    -1.0f,  1.0f, -1.0f,
    -1.0f,  1.0f, -1.0f,
    -1.0f,  1.0f,  1.0f,
    -1.0f, -1.0f,  1.0f,

     1.0f, -1.0f, -1.0f,
     1.0f, -1.0f,  1.0f,
     1.0f,  1.0f,  1.0f,
     1.0f,  1.0f,  1.0f,
     1.0f,  1.0f, -1.0f,
     1.0f, -1.0f, -1.0f,

    -1.0f, -1.0f,  1.0f,
    -1.0f,  1.0f,  1.0f,
     1.0f,  1.0f,  1.0f,
     1.0f,  1.0f,  1.0f,
     1.0f, -1.0f,  1.0f,
    -1.0f, -1.0f,  1.0f,

    -1.0f,  1.0f, -1.0f,
     1.0f,  1.0f, -1.0f,
     1.0f,  1.0f,  1.0f,
     1.0f,  1.0f,  1.0f,
    -1.0f,  1.0f,  1.0f,
    -1.0f,  1.0f, -1.0f,

    -1.0f, -1.0f, -1.0f,
    -1.0f, -1.0f,  1.0f,
     1.0f, -1.0f, -1.0f,
     1.0f, -1.0f, -1.0f,
    -1.0f, -1.0f,  1.0f,
     1.0f, -1.0f,  1.0f
};

namespace {
    // Cubemap face order is +X -X +Y -Y +Z -Z, which GL_TEXTURE_CUBE_MAP_POSITIVE_X + i walks.
    std::vector<std::string> cubemapFaces(const std::string& directory) {
        std::vector<std::string> faces;
        for (const char* face : {"right", "left", "top", "bottom", "front", "back"}) {
            faces.push_back(directory + '/' + face + ".png");
        }
        return faces;
    }
}

Skybox::Skybox() : VAO(0), VBO(0), dayTextureID(0), nightTextureID(0) {
    if (!load(cubemapFaces("assets/textures/skybox/sunny"),
              cubemapFaces("assets/textures/skybox/night"))) {
        std::cerr << "Failed to load skybox!" << std::endl;
    }

    ShaderManager& sm = ShaderManager::getInstance();
    sm.addShader("skybox", "assets/shader/skybox/skybox.vs.glsl",
                               "assets/shader/skybox/skybox.fs.glsl");

    skyboxShader = sm.getShader("skybox");
    if (!skyboxShader) {
        std::cerr << "Failed to get skybox shader!" << std::endl;
        glDepthFunc(GL_LESS);
        return;
    }

    setupMesh();
}

Skybox::~Skybox() {
    glDeleteVertexArrays(1, &VAO);
    glDeleteBuffers(1, &VBO);
    glDeleteTextures(1, &dayTextureID);
    if (nightTextureID != dayTextureID) {
        glDeleteTextures(1, &nightTextureID);
    }
}

void Skybox::setupMesh() {
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);

    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(skyboxVertices), &skyboxVertices, GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);

    glBindVertexArray(0);
}

bool Skybox::load(const std::vector<std::string>& dayFaces, const std::vector<std::string>& nightFaces) {
    dayTextureID = loadCubemap(dayFaces);
    if (dayTextureID == 0) {
        std::cerr << "Day skybox missing" << std::endl;
        return false;
    }

    nightTextureID = loadCubemap(nightFaces);
    if (nightTextureID == 0) {
        std::cerr << "Night skybox missing" << std::endl;
        return false;
    }

    return true;
}

unsigned int Skybox::loadCubemap(const std::vector<std::string>& faces) {
    unsigned int textureID;
    glGenTextures(1, &textureID);
    glBindTexture(GL_TEXTURE_CUBE_MAP, textureID);

    // Tightly packed rows. The default of 4 silently skews any image whose row length in
    // bytes is not a multiple of 4, which is every odd width in RGB.
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

    // Cubemap faces are defined top-left origin, so the global stb flip has to be off.
    stbi_set_flip_vertically_on_load(false);

    int faceSize = 0;

    int width, height, nrChannels;
    for (unsigned int i = 0; i < faces.size(); i++) {
        unsigned char* data = stbi_load(faces[i].c_str(), &width, &height, &nrChannels, 0);
        if (!data) {
            std::cerr << "Cubemap texture failed to load: " << faces[i] << std::endl;
            glDeleteTextures(1, &textureID);
            return 0;
        }

        // Every one of these is checked here because GL will not tell you. A non-square
        // face raises GL_INVALID_VALUE and stores nothing, and faces of differing sizes
        // leave the cubemap incomplete -- both of which render as a black sky with no
        // error anywhere, since the files themselves loaded fine.
        const char* problem = nullptr;
        if (width != height) {
            problem = "is not square";
        } else if (faceSize != 0 && width != faceSize) {
            problem = "does not match the size of the first face";
        } else if (nrChannels != 3 && nrChannels != 4) {
            problem = "has an unsupported channel count";
        }

        if (problem) {
            std::cerr << "Cubemap face " << faces[i] << ' ' << problem
                      << " (" << width << 'x' << height << ", " << nrChannels
                      << " channels";
            if (faceSize != 0) {
                std::cerr << "; first face was " << faceSize << "px";
            }
            std::cerr << ")" << std::endl;

            stbi_image_free(data);
            glDeleteTextures(1, &textureID);
            return 0;
        }

        faceSize = width;

        GLenum format = (nrChannels == 4) ? GL_RGBA : GL_RGB;
        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i,
                     0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
        stbi_image_free(data);
    }

    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

    return textureID;
}

void Skybox::draw(const glm::mat4& view, const glm::mat4& projection, const SunState& sun) {
    glDepthFunc(GL_LEQUAL);

    skyboxShader->use();

    glm::mat4 skyboxView = glm::mat4(glm::mat3(view));

    skyboxShader->setMat4("view", skyboxView);
    skyboxShader->setMat4("projection", projection);
    skyboxShader->setInt("daySkybox", 0);
    skyboxShader->setInt("nightSkybox", 1);
    skyboxShader->setFloat("dayBlend", sun.intensity);

    glBindVertexArray(VAO);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_CUBE_MAP, dayTextureID);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_CUBE_MAP, nightTextureID);
    glDrawArrays(GL_TRIANGLES, 0, 36);
    glBindVertexArray(0);

    // Leave unit 0 current; everything else here assumes it is.
    glActiveTexture(GL_TEXTURE0);

    glDepthFunc(GL_LESS);
}