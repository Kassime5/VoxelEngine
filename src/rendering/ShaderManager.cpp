//
// Created by maxim on 17/01/2026.
//

#include "ShaderManager.h"
#include <iostream>

bool ShaderManager::addShader(const std::string& name, const char* vertexPath,
                               const char* fragmentPath) {
    // Check if shader already exists
    if (shaders.find(name) != shaders.end()) {
        std::cerr << "Shader '" << name << "' already exists!" << std::endl;
        return false;
    }

    auto shader = std::make_unique<Shader>(name, vertexPath, fragmentPath);
    shaders[name] = std::move(shader);

    return true;
}

Shader* ShaderManager::getShader(const std::string& name) {
    auto it = shaders.find(name);
    if (it != shaders.end()) {
        return it->second.get();
    }

    std::cerr << "Shader '" << name << "' not found!" << std::endl;
    return nullptr;
}

bool ShaderManager::hasShader(const std::string& name) const {
    return shaders.find(name) != shaders.end();
}

void ShaderManager::removeShader(const std::string& name) {
    auto it = shaders.find(name);
    if (it != shaders.end()) {
        shaders.erase(it);
    }
}

void ShaderManager::clear() {
    shaders.clear();
}
