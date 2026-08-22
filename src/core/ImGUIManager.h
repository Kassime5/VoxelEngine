//
// Created by maxim on 18/01/2026.
//

#ifndef GLFWVOXEL_IMGUIMANAGER_H
#define GLFWVOXEL_IMGUIMANAGER_H
#include "src/rendering/Camera.h"
#include "src/world/World.h"
#include "src/world/DayCycle.h"
#include "src/debug/RenderStats.h"

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

#include "src/game/Player.h"

class ImGUIManager
{
public:
    ImGUIManager(World& _world, Camera& _camera, int& _renderDistance, Player& _player,
                 DayCycle& _dayCycle);
    ~ImGUIManager();
    void drawImGUIElements(float deltaTime);
private:
    World& world;
    Camera& camera;
    Player& player;
    EntityManager& entityManager;
    DayCycle& dayCycle;

    bool wireframe = false;
    int& renderDistance;

    std::string formatNumber(int number);
};


#endif //GLFWVOXEL_IMGUIMANAGER_H