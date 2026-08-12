//
// Created by maxim on 17/02/2026.
//

#ifndef GLFWVOXEL_MODEL_H
#define GLFWVOXEL_MODEL_H

#include "src/rendering/Shader.h"
#include "src/rendering/Meshes/ModelMesh.h"
#include "src/stb_image.h"

#include <string>
#include <vector>


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

    void loadModel(const std::string& path);
    std::vector<MeshTexture> loadDiffuseTexture(const std::string& file);
};


#endif //GLFWVOXEL_MODEL_H
