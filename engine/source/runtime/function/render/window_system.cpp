#include "window_system.h"

#include "core/base/macro.h"

namespace Vain {

WindowSystem::~WindowSystem() {
    glfwDestroyWindow(m_window);
    glfwTerminate();
}

void WindowSystem::initialize(const WindowCreateInfo &create_info) {
    if (!glfwInit()) {
        VAIN_FATAL("failed to initialize GLFW");
        return;
    }

    m_width = create_info.width;
    m_height = create_info.height;

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    m_window = glfwCreateWindow(
        create_info.width, create_info.height, create_info.title, nullptr, nullptr
    );

    if (!m_window) {
        VAIN_FATAL("failed to create window");
        glfwTerminate();
        return;
    }

    glfwSetWindowUserPointer(m_window, this);

    glfwSetKeyCallback(m_window, keyCallback);
    glfwSetCharCallback(m_window, charCallback);
    glfwSetCharModsCallback(m_window, charModsCallback);
    glfwSetMouseButtonCallback(m_window, mouseButtonCallback);
    glfwSetCursorPosCallback(m_window, cursorPosCallback);
    glfwSetCursorEnterCallback(m_window, cursorEnterCallback);
    glfwSetScrollCallback(m_window, scrollCallback);
    glfwSetDropCallback(m_window, dropCallback);
    glfwSetWindowSizeCallback(m_window, windowSizeCallback);
    glfwSetWindowCloseCallback(m_window, windowCloseCallback);

    glfwSetInputMode(m_window, GLFW_RAW_MOUSE_MOTION, GLFW_FALSE);
}

void WindowSystem::pollEvents() const { glfwPollEvents(); }

bool WindowSystem::shouldClose() const { return glfwWindowShouldClose(m_window); }

void WindowSystem::setTitle(const char *title) const {
    glfwSetWindowTitle(m_window, title);
}

GLFWwindow *WindowSystem::getWindow() const { return m_window; }

std::array<int, 2> WindowSystem::getWindowSize() const {
    return std::array<int, 2>({m_width, m_height});
}

bool WindowSystem::isMouseButtonDown(int button) const {
    if (button < GLFW_MOUSE_BUTTON_1 || button > GLFW_MOUSE_BUTTON_LAST) {
        return false;
    }
    return glfwGetMouseButton(m_window, button) == GLFW_PRESS;
}
void WindowSystem::setInputMode(int mode, int value) const {
    glfwSetInputMode(m_window, mode, value);
}

}  // namespace Vain