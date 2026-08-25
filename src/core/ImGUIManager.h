#ifndef __EMSCRIPTEN__
// ImGui is desktop-only

//
// Created by maxim on 18/01/2026.
//

#ifndef GLFWVOXEL_IMGUIMANAGER_H
#define GLFWVOXEL_IMGUIMANAGER_H
#include "src/rendering/Camera.h"
#include "src/rendering/CloudRenderer.h"
#include "src/debug/TestScene.h"
#include "src/rendering/ShadowMap.h"
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
                 DayCycle& _dayCycle, int& _fpsLimit, ShadowMap& _shadowMap,
                 CloudRenderer& _cloudRenderer);
    ~ImGUIManager();
    void drawImGUIElements(float deltaTime);
private:
    World& world;
    Camera& camera;
    Player& player;
    EntityManager& entityManager;
    DayCycle& dayCycle;
    ShadowMap& shadowMap;
    CloudRenderer& cloudRenderer;

    bool wireframe = false;
    int& renderDistance;
    int& fpsLimit;

    std::string formatNumber(int number);
    void drawShadowSettings();
    void drawCloudSettings();
    void drawTestScene();
    void goTo(const TestScene::Viewpoint& view);
};


#endif //GLFWVOXEL_IMGUIMANAGER_H

#endif
