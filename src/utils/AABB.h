//
// Created by maxim on 15/02/2026.
//

#ifndef GLFWVOXEL_AABB_H
#define GLFWVOXEL_AABB_H
#include "glm/vec3.hpp"

struct AABB {
    glm::vec3 min;
    glm::vec3 max;

    bool intersects(const AABB& other) const {
        return (min.x <= other.max.x && max.x >= other.min.x) &&
               (min.y <= other.max.y && max.y >= other.min.y) &&
               (min.z <= other.max.z && max.z >= other.min.z);
    }
};

#endif //GLFWVOXEL_AABB_H