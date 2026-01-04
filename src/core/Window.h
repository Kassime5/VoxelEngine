//
// Created by maxim on 04/01/2026.
//

#ifndef GLFWVOXEL_WINDOW_H
#define GLFWVOXEL_WINDOW_H
#include "GLFW/glfw3.h"
#include <iostream>


class Window {
public:
    Window(unsigned int width = 800, unsigned int height = 600, const char * name = nullptr, GLFWmonitor* monitor = nullptr, GLFWwindow* share = nullptr);
    ~Window();

    void setInputMode(int mode, int value) const;
    void setSwapInterval(int interval);

    [[nodiscard]] GLFWwindow* getWindow() const { return window; }

    operator GLFWwindow*() const { return window; }
private:
    GLFWwindow * window;
};


#endif //GLFWVOXEL_WINDOW_H