//
// Created by maxim on 17/01/2026.
//

#ifndef GLFWVOXEL_SHADERMANAGER_H
#define GLFWVOXEL_SHADERMANAGER_H

#include <unordered_map>
#include <memory>
#include <string>
#include "Shader.h"

class ShaderManager {
public:
    static ShaderManager& getInstance() {
        static ShaderManager instance;
        return instance;
    }

    // Singleton stuff
    ShaderManager(const ShaderManager&) = delete;
    ShaderManager& operator=(const ShaderManager&) = delete;
    ShaderManager(ShaderManager&&) = delete;
    ShaderManager& operator=(ShaderManager&&) = delete;
    ~ShaderManager() = default;

    bool addShader(const std::string& name, const char* vertexPath,
        const char* fragmentPath);

    Shader* getShader(const std::string& name);
    bool hasShader(const std::string& name) const;
    void removeShader(const std::string& name);
    void clear();
    size_t getShaderCount() const { return shaders.size(); }

private:
    ShaderManager() = default;
    std::unordered_map<std::string, std::unique_ptr<Shader>> shaders;
};

#endif //GLFWVOXEL_SHADERMANAGER_H