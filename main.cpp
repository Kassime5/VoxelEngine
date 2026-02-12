#define GLFW_INCLUDE_NONE
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <iostream>
#include "src/world/World.h"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include "src/core/Window.h"

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

#include "src/core/ImGUIManager.h"
#include "src/debug/RenderStats.h"
#include "src/game/Player.h"
#include "src/game/HighlightBox.h"
#include "src/game/SoundManager.h"
#include "src/input/PlayerController.h"
#include "src/rendering/ShaderManager.h"
#include "src/rendering/Skybox.h"
#include "thirdparty/AL/al.h"
#include "thirdparty/AL/alc.h"

void framebuffer_size_callback(GLFWwindow *window, int width, int height);

// Window Settings
const unsigned int SCR_WIDTH = 1920;
const unsigned int SCR_HEIGHT = 1080;
bool vsync = false;

// Engine variables
float deltaTime = 0.0f;
float lastFrame = 0.0f;
int renderDistance = 8;
SoundManager* soundManager;

// Camera variables
Player player(glm::vec3(0, 300, 0));
float lastX = SCR_WIDTH / 2.0f;
float lastY = SCR_HEIGHT / 2.0f;
bool firstMouse = true;

bool isCursorVisible = false;
ALCdevice *device;

int main() {
    // glfw: initialize and configure
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    Window *window = new Window(SCR_WIDTH, SCR_HEIGHT, "OpenGL");

    soundManager = new SoundManager(&player);
    if (!soundManager->initialize()) {
        std::cerr << "Failed to initialize sound system!" << std::endl;
    }

    // Sound stuff, TODO: move to a proper game manager
    ALuint ambianceBuffer = soundManager->loadSound("assets/music/MusicAmbianceMono.wav");
    soundManager->playSound3D(ambianceBuffer, 0.0f, 55.0f, 0.0f, true, 0.3f);

    glfwMakeContextCurrent(*window);
    window->setSwapInterval(vsync ? 1 : 0);

    InputManager::getInstance().initialize(*window);
    glfwSetFramebufferSizeCallback(*window, framebuffer_size_callback);

    // glad: load all OpenGL function pointers
    if (!gladLoadGLLoader((GLADloadproc) glfwGetProcAddress)) {
        std::cout << "Failed to initialize GLAD" << std::endl;
        return -1;
    }

    Skybox skybox;

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // Create world and load texture atlas
    World* world = new World(&player);
    world->setRenderDistance(renderDistance);

    PlayerController playerController(&player, world);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOpenGL(*window, true);
    ImGui_ImplOpenGL3_Init("#version 460");
    ImGUIManager* imGUIManager = new ImGUIManager(world, &player.getCamera(), &renderDistance, &player);

    HighlightBox highlightBox;
    RaycastResult highlightedBlock;

    while (!glfwWindowShouldClose(*window)) {
        glfwPollEvents();

        float currentFrame = glfwGetTime();
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        RenderStats::getInstance().resetFrame();

        glClearColor(0.2f, 0.3f, 0.3f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        InputManager::getInstance().update();
        player.update(deltaTime, world);
        playerController.processInput(deltaTime);
        world->update(player.getPosition());
        soundManager->update();

        glm::mat4 projection = glm::perspective(glm::radians(player.getCamera().Zoom),
            (float)SCR_WIDTH / (float)SCR_HEIGHT, 0.1f, 1000.0f);
        glm::mat4 view = player.getCamera().GetViewMatrix();

        world->renderWorld(projection, view);

        highlightedBlock = world->raycastBlock(player.getCamera().Position, player.getFront(), 5.0f);
        if (highlightedBlock.hit) {
            highlightBox.draw(highlightedBlock, projection, view);
        }

        skybox.draw(player.getCamera().GetViewMatrix(), projection);

        imGUIManager->drawImGUIElements(deltaTime);
        glfwSwapBuffers(*window);
        InputManager::getInstance().endFrame();
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwTerminate();

    soundManager->shutdown();
    delete soundManager;
    delete window;
    delete world;

    return 0;
}

void framebuffer_size_callback(GLFWwindow *window, int width, int height) {
    glViewport(0, 0, width, height);
}
