#define GLFW_INCLUDE_NONE
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <iostream>
#include "src/rendering/Shader.h"
#include "src/rendering/Camera.h"
#include "src/world/World.h"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include "src/core/Window.h"

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include "src/debug/RenderStats.h"
#include "src/rendering/Skybox.h"

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

bool wireframe = false;

// Camera variables
Camera camera(glm::vec3(0.0f, 100.0f, 0.0f));
float lastX = SCR_WIDTH / 2.0f;
float lastY = SCR_HEIGHT / 2.0f;
bool firstMouse = true;

bool isCursorVisible = false;


int main() {
    // glfw: initialize and configure
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    Window *window = new Window(SCR_WIDTH, SCR_HEIGHT, "OpenGL");

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
    std::vector<std::string> faces = {
        "assets/textures/skybox/right.png",
        "assets/textures/skybox/left.png",
        "assets/textures/skybox/top.png",
        "assets/textures/skybox/bottom.png",
        "assets/textures/skybox/front.png",
        "assets/textures/skybox/back.png"
    };
    if (!skybox.load(faces)) {
        std::cerr << "Failed to load skybox!" << std::endl;
    }

    // configure global opengl state
    glEnable(GL_DEPTH_TEST);

    Shader terrainShader("assets/shader/terrain/terrain_vertex.shader", "assets/shader/terrain/terrain_fragment.shader");


    // Create world and load texture atlas
    World* world = new World();
    world->setRenderDistance(renderDistance);

    if (!world->loadTextureAtlas("assets/textures/atlas.png", 8)) {
        std::cerr << "Failed to load texture atlas!" << std::endl;
        return -1;
    }

    terrainShader.use();
    terrainShader.setFloat("tilesPerRow", 8.0f);
    terrainShader.setInt("texture1", 0);

    // TODO: Change where the lightsource is
    terrainShader.setVec3("lightPos", glm::vec3(0.0f, 50.0f, 0.0f));
    terrainShader.setVec3("lightColor", glm::vec3(1.0f, 1.0f, 1.0f));

    glm::mat4 projection = glm::perspective(glm::radians(45.0f), (float) SCR_WIDTH / (float) SCR_HEIGHT, 0.1f, 100.0f);
    terrainShader.setMat4("projection", projection);

    // world.update(camera.Position);

    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOpenGL(*window, true);
    ImGui_ImplOpenGL3_Init("#version 460");

    int frameCount = 0;

    while (!glfwWindowShouldClose(*window)) {
        float currentFrame = glfwGetTime();
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;
        // frameCount++;

        RenderStats::getInstance().resetFrame();

        // if (frameCount % 100 == 0){
        //     Profiler::getInstance().printStats();
        //     // world->printDebugInfo();
        // }

        processInput(*window, world);

        glClearColor(0.2f, 0.3f, 0.3f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        world->update(camera.Position);
        terrainShader.use();

        terrainShader.setVec3("viewPos", camera.Position);

        glm::mat4 projection = glm::perspective(glm::radians(camera.Zoom),
            (float)SCR_WIDTH / (float)SCR_HEIGHT, 0.1f, 1000.0f);
        terrainShader.setMat4("projection", projection);

        glm::mat4 view = camera.GetViewMatrix();
        terrainShader.setMat4("view", view);

        world->render(terrainShader);

        skybox.draw(camera.GetViewMatrix(), projection);


        // ImGUI Rendering
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // Main debug window
        ImGui::Begin("Debug Info");

        // Performance stats
        ImGui::SeparatorText("Performance");
        ImGui::Text("FPS: %.1f", 1.0f / deltaTime);
        ImGui::Text("Frame Time: %.3f ms", deltaTime * 1000.0f);

        // Rendering stats
        ImGui::SeparatorText("Rendering");
        auto& stats = RenderStats::getInstance();
        ImGui::Text("Draw Calls: %d", stats.getDrawCalls());
        ImGui::Text("Triangles: %s", formatNumber(stats.getTriangles()).c_str());
        ImGui::Text("Vertices: %s", formatNumber(stats.getVertices()).c_str());
        ImGui::Text("Avg Tri/Draw: %.1f", stats.getAvgTrianglesPerDrawCall());

        // Chunk stats
        ImGui::SeparatorText("Chunks");
        ImGui::Text("Rendered: %d", stats.getChunksRendered());
        ImGui::Text("Skipped: %d", stats.getChunksSkipped());
        ImGui::Text("Total Loaded: %d", world->getLoadedChunkCount());

        // Memory estimate
        ImGui::SeparatorText("Memory (Estimate)");
        float vertexMemoryMB = (stats.getVertices() * sizeof(Vertex)) / (1024.0f * 1024.0f);
        float indexMemoryMB = (stats.getTriangles() * 3 * sizeof(unsigned int)) / (1024.0f * 1024.0f);
        ImGui::Text("Vertex Data: %.2f MB", vertexMemoryMB);
        ImGui::Text("Index Data: %.2f MB", indexMemoryMB);
        ImGui::Text("Total GPU: %.2f MB", vertexMemoryMB + indexMemoryMB);

        // Settings
        ImGui::SeparatorText("Settings");
        if (ImGui::SliderInt("Render Distance", &renderDistance, 2, 32)) {
            world->setRenderDistance(renderDistance);
        }

        ImGui::Checkbox("Wireframe", &wireframe);
        if (wireframe) {
            glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
        } else {
            glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
        }

        ImGui::End();
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        // ---

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

    bool sprinting = false;
    if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS)
        sprinting = true;

    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
        camera.ProcessKeyboard(FORWARD, sprinting, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
        camera.ProcessKeyboard(BACKWARD, sprinting, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
        camera.ProcessKeyboard(LEFT, sprinting, deltaTime);
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
        camera.ProcessKeyboard(RIGHT, sprinting, deltaTime);

    if (glfwGetKey(window, GLFW_KEY_Z) == GLFW_PRESS) {
        wireframe = !wireframe;
        glPolygonMode(GL_FRONT_AND_BACK, wireframe ? GL_LINE : GL_FILL);
    }

    if (glfwGetKey(window, GLFW_KEY_X) == GLFW_PRESS) {
        std::cout << camera.Position.x << "-" << camera.Position.y << "-" << camera.Position.z << std::endl;
        BlockType type = world->getBlock(camera.Position.x, camera.Position.y, camera.Position.z);
        std::cout << printBlockType(type) << std::endl;
    }

    if (glfwGetKey(window, GLFW_KEY_C) == GLFW_PRESS && !keyC) {
        keyC = true;
        world->setBlock(camera.Position.x, camera.Position.y, camera.Position.z, BlockType::Sand);
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

    camera.ProcessMouseMovement(xoffset, yoffset);
}

void scroll_callback(GLFWwindow* window, double xoffset, double yoffset)
{
    camera.ProcessMouseScroll(static_cast<float>(yoffset));
}

std::string formatNumber(int number) {
    std::string str = std::to_string(number);
    int insertPosition = str.length() - 3;
    while (insertPosition > 0) {
        str.insert(insertPosition, ",");
        insertPosition -= 3;
    }
    return str;
}

