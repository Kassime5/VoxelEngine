//
// Created by maxim on 04/01/2026.
//

#include "Window.h"

Window::Window(unsigned int width, unsigned int height, const char *name, GLFWmonitor *monitor, GLFWwindow *share) {
    window = glfwCreateWindow(width, height, name, monitor, share);
    if (window == nullptr) {
        std::cout << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
    }
}

Window::~Window() {
    delete &window;
}

void Window::setInputMode(int mode, int value) const {
    glfwSetInputMode(window, GLFW_CURSOR, value);
}

void Window::setSwapInterval(int interval) {
    glfwSwapInterval(interval);
}
