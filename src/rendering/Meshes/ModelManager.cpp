//
// Created by maxim on 17/02/2026.
//

#include "ModelManager.h"

bool ModelManager::addModel(const std::string& name, std::string const& path, bool gamma)
{
    if (models.find(name) != models.end())
    {
        std::cout << "Model '" << name << "' already exists!" << std::endl;
        return false;
    }

    auto model = std::make_unique<Model>(path, gamma);
    models[name] = std::move(model);

    return true;
}

Model* ModelManager::getModel(const std::string& name)
{
    auto it = models.find(name);
    if (it != models.end()) {
        return it->second.get();
    }

    std::cerr << "Model '" << name << "' not found!" << std::endl;
    return nullptr;
}
