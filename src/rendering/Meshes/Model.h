//
// Created by maxim on 17/02/2026.
//

#ifndef GLFWVOXEL_MODEL_H
#define GLFWVOXEL_MODEL_H

#include "src/rendering/Shader.h"
#include "src/rendering/Meshes/ModelMesh.h"
#include "assimp/material.h"
#include "assimp/scene.h"
#include "src/stb_image.h"


unsigned int TextureFromFile(const char* path, const std::string& directory);

class Model
{
public:
    Model(std::string const &path, bool gamma = false) : gammaCorrection(gamma)
    {
        loadModel(path);
    }

    void Draw(Shader &shader);
private:
    std::vector<ModelMesh> meshes;
    std::string directory;
    std::vector<MeshTexture> textures_loaded;
    bool gammaCorrection;

    void loadModel(std::string path);
    void processNode(aiNode *node, const aiScene *scene);
    ModelMesh processMesh(aiMesh *mesh, const aiScene *scene);
    std::vector<MeshTexture> loadMaterialTextures(aiMaterial *mat, aiTextureType type, std::string typeName);
};


#endif //GLFWVOXEL_MODEL_H