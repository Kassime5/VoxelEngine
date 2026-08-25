//
// Created by maxim on 25/08/2026.
//

#ifndef GLFWVOXEL_TESTSCENE_H
#define GLFWVOXEL_TESTSCENE_H

#include <glm/glm.hpp>

// A flat world with fixed props
namespace TestScene
{
    extern bool enabled;

    constexpr int GROUND_Y = 64;
    constexpr int LONE_BLOCK_X = 0;
    constexpr int LONE_BLOCK_Z = -18;

    constexpr int WALL_X = 0;
    constexpr int WALL_Z0 = -12, WALL_Z1 = -3;
    constexpr int WALL_TOP = GROUND_Y + 5;

    constexpr int CUBE_X0 = 0, CUBE_X1 = 7;
    constexpr int CUBE_Z0 = 2, CUBE_Z1 = 9;
    constexpr int CUBE_TOP = GROUND_Y + 8;

    constexpr int STAIR_X0 = 0;
    constexpr int STAIR_Z0 = 13, STAIR_Z1 = 17;

    constexpr int SLAB_X0 = 0, SLAB_X1 = 2;
    constexpr int SLAB_Z0 = 21, SLAB_Z1 = 27;
    constexpr int SLAB_Y = GROUND_Y + 4;

    constexpr int GRAZE_WALL_Z = 40;
    constexpr int GRAZE_WALL_X0 = -24, GRAZE_WALL_X1 = 24;
    constexpr int GRAZE_WALL_TOP = GROUND_Y + 20;

    struct Viewpoint {
        glm::vec3 position;
        float yaw;
        float pitch;
    };


    constexpr Viewpoint WALL_VIEW{{0.0f, 74.0f, 62.0f}, -90.0f, -8.0f};
    constexpr Viewpoint GROUND_VIEW{{-40.0f, 72.0f, 0.0f}, 0.0f, -20.0f};
    constexpr Viewpoint OVERVIEW{{-46.0f, 88.0f, 30.0f}, -35.0f, -28.0f};
}

#endif //GLFWVOXEL_TESTSCENE_H
