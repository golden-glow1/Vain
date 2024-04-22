#pragma once

#include <glm/glm.hpp>
#include <vector>

namespace Vain {

enum class EditorCommand : uint32_t {
    camera_left = 1 << 0,       // A
    camera_back = 1 << 1,       // S
    camera_foward = 1 << 2,     // W
    camera_right = 1 << 3,      // D
    camera_up = 1 << 4,         // Q
    camera_down = 1 << 5,       // E
    exit = 1 << 6,              // Esc

};

class EditorInputManager {
  public:
    EditorInputManager() = default;
    ~EditorInputManager();

    void initialize();
    void clear();

    void tick(float delta_time);

  private:
    glm::vec2 m_engine_window_pos{0.0f, 0.0f};
    glm::vec2 m_engine_window_size{1280.0f, 768.0f};
    float m_mouse_x{0.0f};
    float m_mouse_y{0.0f};
    float m_camera_speed{0.05f};

    uint32_t m_editor_command{0};

    void registerInput();

    void onKey(int key, int scancode, int action, int mods);
    void onCursorPos(double xpos, double ypos);
    void onCursorEnter(int entered);
    void onScroll(double xoffset, double yoffset);

    void processEditorCommand();
};

}  // namespace Vain