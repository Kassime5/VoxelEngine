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
#include "src/rendering/ShaderManager.h"
#include "src/rendering/Skybox.h"
#include "thirdparty/AL/al.h"
#include "thirdparty/AL/alc.h"

void framebuffer_size_callback(GLFWwindow *window, int width, int height);
void processInput(GLFWwindow *window, World *world);
void mouse_callback(GLFWwindow *window, double xposIn, double yposIn);
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset);
std::string formatNumber(int number);

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
    window->setInputMode(GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    glfwSetFramebufferSizeCallback(*window, framebuffer_size_callback);
    glfwSetCursorPosCallback(*window, mouse_callback);
    glfwSetScrollCallback(*window, scroll_callback);

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

    if (!world->loadTextureAtlas("assets/textures/atlas2.png", 8)) {
        std::cerr << "Failed to load texture atlas!" << std::endl;
        return -1;
    }

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
        float currentFrame = glfwGetTime();
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        RenderStats::getInstance().resetFrame();

        processInput(*window, world);

        glClearColor(0.2f, 0.3f, 0.3f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        player.update(deltaTime, world);
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

        // TODO: Change this part
        world->setRenderDistance(renderDistance);

        glfwSwapBuffers(*window);
        glfwPollEvents();
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    // glfw: terminate, clearing all previously allocated GLFW resources.
    glfwTerminate();

    soundManager->shutdown();
    delete soundManager;
    delete window;
    delete world;

    return 0;
}

bool keyC = false;
bool keyEsc = false;

void framebuffer_size_callback(GLFWwindow *window, int width, int height) {
    glViewport(0, 0, width, height);
}

void processInput(GLFWwindow *window, World *world) {

    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS && !keyEsc) {
        keyEsc = true;
        isCursorVisible = !isCursorVisible;
        if (!isCursorVisible) {
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
        } else {
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
        }
    } else if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_RELEASE) {
        keyEsc = false;
    }

    static bool keyF = false;
    if (glfwGetKey(window, GLFW_KEY_F) == GLFW_PRESS && !keyF) {
        keyF = true;
        if (player.isFlying()) {
            player.deactivateFlying();
        } else {
            player.activateFlying();
        }
    } else if (glfwGetKey(window, GLFW_KEY_F) == GLFW_RELEASE) {
        keyF = false;
    }

    bool sprinting = false;
    if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS)
        sprinting = true;

    bool forward = glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS;
    bool backward = glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS;
    bool left = glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS;
    bool right = glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS;

    player.processMovement(forward, backward, left, right, sprinting, deltaTime);

    if (player.isFlying()) {
        bool ascending = glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS;
        bool descending = glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS;
        player.processVerticalInput(ascending, descending, deltaTime);
    } else {
        // Jump when not flying
        static bool keySpace = false;
        if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS && !keySpace) {
            keySpace = true;
            player.jump();
        } else if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_RELEASE) {
            keySpace = false;
        }
    }

    if (glfwGetKey(window, GLFW_KEY_X) == GLFW_PRESS) {
        std::cout << player.getPosition().x << "-" << player.getPosition().y << "-" << player.getPosition().z << std::endl;
        BlockType type = world->getBlock(player.getPosition().x, player.getPosition().y, player.getPosition().z);
        std::cout << printBlockType(type) << std::endl;
    }

    if (glfwGetKey(window, GLFW_KEY_C) == GLFW_PRESS && !keyC) {
        keyC = true;
        world->setBlock(player.getPosition().x, player.getPosition().y, player.getPosition().z, BlockType::Grass);
    } else if (glfwGetKey(window, GLFW_KEY_C) == GLFW_RELEASE) {
        keyC = false;
    }
}

void mouse_callback(GLFWwindow* window, double xposIn, double yposIn)
{
    if (isCursorVisible == true) return;

    float xpos = static_cast<float>(xposIn);
    float ypos = static_cast<float>(yposIn);

    if (firstMouse)
    {
        lastX = xpos;
        lastY = ypos;
        firstMouse = false;
    }

    float xoffset = xpos - lastX;
    float yoffset = lastY - ypos;

    lastX = xpos;
    lastY = ypos;

    player.getCamera().ProcessMouseMovement(xoffset, yoffset);
}

void scroll_callback(GLFWwindow* window, double xoffset, double yoffset)
{
    player.getCamera().ProcessMouseScroll(static_cast<float>(yoffset));
}
