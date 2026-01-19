//
// Created by maxim on 18/01/2026.
//

#ifndef GLFWVOXEL_IMGUIMANAGER_H
#define GLFWVOXEL_IMGUIMANAGER_H
#include "src/rendering/Camera.h"
#include "src/world/World.h"
#include "src/debug/RenderStats.h"

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

class ImGUIManager
{
public:
    ImGUIManager(World* _world, Camera* _camera, int* _renderDistance);
    ~ImGUIManager();
    void drawImGUIElements(float deltaTime);
private:
    World* world;
    Camera* camera;
    bool wireframe = false;
    int* renderDistance;

    std::string formatNumber(int number);
};


#endif //GLFWVOXEL_IMGUIMANAGER_H