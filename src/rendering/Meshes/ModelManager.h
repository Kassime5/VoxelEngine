//
// Created by maxim on 17/02/2026.
//

#ifndef GLFWVOXEL_MESHMANAGER_H
#define GLFWVOXEL_MESHMANAGER_H
#include <memory>

#include "Model.h"


class ModelManager
{
    public:
    static ModelManager& getInstance() {
        static ModelManager instance;
        return instance;
    }

    ModelManager(const ModelManager&) = delete;
    ModelManager& operator=(const ModelManager&) = delete;
    ModelManager(ModelManager&&) = delete;
    ModelManager& operator=(ModelManager&&) = delete;
    ~ModelManager() = default;

    bool addModel(const std::string& name, std::string const& path, bool gamma = false);

    Model* getModel(const std::string& name);
    size_t getModelCount() const { return models.size(); }

private:
    ModelManager() = default;
    std::unordered_map<std::string, std::unique_ptr<Model>> models;
};

#endif //GLFWVOXEL_MESHMANAGER_H