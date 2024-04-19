#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <array>
#include <functional>
#include <vector>

namespace Vain {

struct WindowCreateInfo {
    int width{1280};
    int height{720};
    const char *title{"Vain"};
    bool is_fullscreen{false};
};

class WindowSystem {
  public:
    using OnResetFunc = std::function<void()>;
    using OnKeyFunc = std::function<void(int, int, int, int)>;
    using OnCharFunc = std::function<void(unsigned int)>;
    using OnCharModsFunc = std::function<void(int, unsigned int)>;
    using OnMouseButtonFunc = std::function<void(int, int, int)>;
    using OnCursorPosFunc = std::function<void(double, double)>;
    using OnCursorEnterFunc = std::function<void(int)>;
    using OnScrollFunc = std::function<void(double, double)>;
    using OnDropFunc = std::function<void(int, const char **)>;
    using OnWindowSizeFunc = std::function<void(int, int)>;

    WindowSystem() = default;
    ~WindowSystem();
    void initialize(const WindowCreateInfo &create_info);
    void pollEvents() const;
    bool shouldClose() const;
    void setTitle(const char *title) const;
    GLFWwindow *getWindow() const;
    std::array<int, 2> getWindowSize() const;

    void registerOnResetFunc(OnResetFunc func) { m_onResetFunc.push_back(func); }
    void registerOnKeyFunc(OnKeyFunc func) { m_onKeyFunc.push_back(func); }
    void registerOnCharFunc(OnCharFunc func) { m_onCharFunc.push_back(func); }
    void registerOnCharModsFunc(OnCharModsFunc func) { m_onCharModsFunc.push_back(func); }
    void registerOnMouseButtonFunc(OnMouseButtonFunc func) {
        m_onMouseButtonFunc.push_back(func);
    }
    void registerOnCursorPosFunc(OnCursorPosFunc func) {
        m_onCursorPosFunc.push_back(func);
    }
    void registerOnCursorEnterFunc(OnCursorEnterFunc func) {
        m_onCursorEnterFunc.push_back(func);
    }
    void registerOnScrollFunc(OnScrollFunc func) { m_onScrollFunc.push_back(func); }
    void registerOnDropFunc(OnDropFunc func) { m_onDropFunc.push_back(func); }
    void registerOnWindowSizeFunc(OnWindowSizeFunc func) {
        m_onWindowSizeFunc.push_back(func);
    }

  private:
    GLFWwindow *m_window{nullptr};
    int m_width{0};
    int m_height{0};

    bool m_is_focus_mode{false};

    // function pointers
    std::vector<OnResetFunc> m_onResetFunc;
    std::vector<OnKeyFunc> m_onKeyFunc;
    std::vector<OnCharFunc> m_onCharFunc;
    std::vector<OnCharModsFunc> m_onCharModsFunc;
    std::vector<OnMouseButtonFunc> m_onMouseButtonFunc;
    std::vector<OnCursorPosFunc> m_onCursorPosFunc;
    std::vector<OnCursorEnterFunc> m_onCursorEnterFunc;
    std::vector<OnScrollFunc> m_onScrollFunc;
    std::vector<OnDropFunc> m_onDropFunc;
    std::vector<OnWindowSizeFunc> m_onWindowSizeFunc;

    // window event callbacks
    static void keyCallback(
        GLFWwindow *window, int key, int scancode, int action, int mods
    ) {
        WindowSystem *app =
            reinterpret_cast<WindowSystem *>(glfwGetWindowUserPointer(window));
        if (app) {
            app->onKey(key, scancode, action, mods);
        }
    }

    static void charCallback(GLFWwindow *window, unsigned int codepoint) {
        WindowSystem *app =
            reinterpret_cast<WindowSystem *>(glfwGetWindowUserPointer(window));
        if (app) {
            app->onChar(codepoint);
        }
    }

    static void charModsCallback(GLFWwindow *window, unsigned int codepoint, int mods) {
        WindowSystem *app =
            reinterpret_cast<WindowSystem *>(glfwGetWindowUserPointer(window));
        if (app) {
            app->onCharMods(codepoint, mods);
        }
    }

    static void mouseButtonCallback(
        GLFWwindow *window, int button, int action, int mods
    ) {
        WindowSystem *app =
            reinterpret_cast<WindowSystem *>(glfwGetWindowUserPointer(window));
        if (app) {
            app->onMouseButton(button, action, mods);
        }
    }

    static void cursorPosCallback(GLFWwindow *window, double xpos, double ypos) {
        WindowSystem *app =
            reinterpret_cast<WindowSystem *>(glfwGetWindowUserPointer(window));
        if (app) {
            app->onCursorPos(xpos, ypos);
        }
    }

    static void cursorEnterCallback(GLFWwindow *window, int entered) {
        WindowSystem *app =
            reinterpret_cast<WindowSystem *>(glfwGetWindowUserPointer(window));
        if (app) {
            app->onCursorEnter(entered);
        }
    }

    static void scrollCallback(GLFWwindow *window, double xoffset, double yoffset) {
        WindowSystem *app =
            reinterpret_cast<WindowSystem *>(glfwGetWindowUserPointer(window));
        if (app) {
            app->onScroll(xoffset, yoffset);
        }
    }

    static void dropCallback(GLFWwindow *window, int count, const char **paths) {
        WindowSystem *app =
            reinterpret_cast<WindowSystem *>(glfwGetWindowUserPointer(window));
        if (app) {
            app->onDrop(count, paths);
        }
    }

    static void windowSizeCallback(GLFWwindow *window, int width, int height) {
        WindowSystem *app =
            reinterpret_cast<WindowSystem *>(glfwGetWindowUserPointer(window));
        if (app) {
            app->m_width = width;
            app->m_height = height;
            app->onWindowSize(width, height);
        }
    }

    static void windowCloseCallback(GLFWwindow *window) {
        glfwSetWindowShouldClose(window, true);
    }

    void onReset() {
        for (auto &func : m_onResetFunc) {
            func();
        }
    }

    void onKey(int key, int scancode, int action, int mods) {
        for (auto &func : m_onKeyFunc) {
            func(key, scancode, action, mods);
        }
    }

    void onChar(unsigned int codepoint) {
        for (auto &func : m_onCharFunc) {
            func(codepoint);
        }
    }

    void onCharMods(int codepoint, unsigned int mods) {
        for (auto &func : m_onCharModsFunc) {
            func(codepoint, mods);
        }
    }

    void onMouseButton(int button, int action, int mods) {
        for (auto &func : m_onMouseButtonFunc) {
            func(button, action, mods);
        }
    }

    void onCursorPos(double xpos, double ypos) {
        for (auto &func : m_onCursorPosFunc) {
            func(xpos, ypos);
        }
    }

    void onCursorEnter(int entered) {
        for (auto &func : m_onCursorEnterFunc) {
            func(entered);
        }
    }

    void onScroll(double xoffset, double yoffset) {
        for (auto &func : m_onScrollFunc) {
            func(xoffset, yoffset);
        }
    }

    void onDrop(int count, const char **paths) {
        for (auto &func : m_onDropFunc) {
            func(count, paths);
        }
    }

    void onWindowSize(int width, int height) {
        for (auto &func : m_onWindowSizeFunc) {
            func(width, height);
        }
    }
};

}  // namespace Vain