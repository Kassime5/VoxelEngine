//
// Created by maxim on 15/02/2026.
//

#ifndef GLFWVOXEL_VECTORHASH_H
#define GLFWVOXEL_VECTORHASH_H

#include <glm/glm.hpp>
#include <functional>

struct IVec3Hash {
    std::size_t operator()(const glm::ivec3& v) const {
        std::size_t h1 = std::hash<int>()(v.x);
        std::size_t h2 = std::hash<int>()(v.y);
        std::size_t h3 = std::hash<int>()(v.z);
        return h1 ^ (h2 << 1) ^ (h3 << 2);
    }
};

#endif //GLFWVOXEL_VECTORHASH_H