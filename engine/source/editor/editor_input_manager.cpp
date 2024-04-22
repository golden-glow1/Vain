#include "editor_input_manager.h"

#include <function/render/render_system.h>
#include <function/render/window_system.h>

#include "editor_global_context.h"

namespace Vain {

uint32_t k_complement_control_command = 0xFFFFFFFF;

EditorInputManager::~EditorInputManager() { clear(); }

void EditorInputManager::initialize() { registerInput(); }

void EditorInputManager::clear() {}

void EditorInputManager::tick(float delta_time) { processEditorCommand(); }

void EditorInputManager::registerInput() {
    g_editor_global_context.window_system->registerOnKeyFunc(
        [this](int key, int scancode, int action, int mods) {
            onKey(key, scancode, action, mods);
        }
    );
    g_editor_global_context.window_system->registerOnCursorPosFunc(
        [this](double xpos, double ypos) { onCursorPos(xpos, ypos); }
    );
    g_editor_global_context.window_system->registerOnCursorEnterFunc([this](int entered) {
        onCursorEnter(entered);
    });
    g_editor_global_context.window_system->registerOnScrollFunc(
        [this](double xoffset, double yoffset) { onScroll(xoffset, yoffset); }
    );
}

void EditorInputManager::onKey(int key, int scancode, int action, int mods) {
    if (action == GLFW_PRESS) {
        switch (key) {
        case GLFW_KEY_A:
            m_editor_command |= (uint32_t)EditorCommand::camera_left;
            break;
        case GLFW_KEY_S:
            m_editor_command |= (uint32_t)EditorCommand::camera_back;
            break;
        case GLFW_KEY_W:
            m_editor_command |= (uint32_t)EditorCommand::camera_foward;
            break;
        case GLFW_KEY_D:
            m_editor_command |= (uint32_t)EditorCommand::camera_right;
            break;
        case GLFW_KEY_Q:
            m_editor_command |= (uint32_t)EditorCommand::camera_up;
            break;
        case GLFW_KEY_E:
            m_editor_command |= (uint32_t)EditorCommand::camera_down;
            break;
        default:
            break;
        }
    } else if (action == GLFW_RELEASE) {
        switch (key) {
        case GLFW_KEY_ESCAPE:
            m_editor_command &=
                (k_complement_control_command ^ (uint32_t)EditorCommand::exit);
            break;
        case GLFW_KEY_A:
            m_editor_command &=
                (k_complement_control_command ^ (uint32_t)EditorCommand::camera_left);
            break;
        case GLFW_KEY_S:
            m_editor_command &=
                (k_complement_control_command ^ (uint32_t)EditorCommand::camera_back);
            break;
        case GLFW_KEY_W:
            m_editor_command &=
                (k_complement_control_command ^ (uint32_t)EditorCommand::camera_foward);
            break;
        case GLFW_KEY_D:
            m_editor_command &=
                (k_complement_control_command ^ (uint32_t)EditorCommand::camera_right);
            break;
        case GLFW_KEY_Q:
            m_editor_command &=
                (k_complement_control_command ^ (uint32_t)EditorCommand::camera_up);
            break;
        case GLFW_KEY_E:
            m_editor_command &=
                (k_complement_control_command ^ (uint32_t)EditorCommand::camera_down);
            break;
        default:
            break;
        }
    }
}

void EditorInputManager::onCursorPos(double xpos, double ypos) {
    float dimension = std::max(m_engine_window_size.x, m_engine_window_size.y);
    float angular_velocity = 180.0 / dimension;
    auto window_system = g_editor_global_context.window_system;

    if (m_mouse_x >= 0.0f && m_mouse_y >= 0.0f) {
        if (window_system->isMouseButtonDown(GLFW_MOUSE_BUTTON_RIGHT)) {
            window_system->setInputMode(GLFW_CURSOR, GLFW_CURSOR_DISABLED);
            g_editor_global_context.render_system->getRenderCamera()->rotate(
                glm::vec2{ypos - m_mouse_y, xpos - m_mouse_x} * angular_velocity
            );
        } else {
            window_system->setInputMode(GLFW_CURSOR, GLFW_CURSOR_NORMAL);
        }
    }

    m_mouse_x = xpos;
    m_mouse_y = ypos;
}

void EditorInputManager::onCursorEnter(int entered) {
    if (!entered) {
        m_mouse_x = m_mouse_y = -1.0f;
    }
}

void EditorInputManager::onScroll(double xoffset, double yoffset) {}

void EditorInputManager::processEditorCommand() {
    float camera_speed = m_camera_speed;
    auto camera = g_editor_global_context.render_system->getRenderCamera();
    glm::vec3 delta{0.0};

    if ((uint32_t)EditorCommand::camera_foward & m_editor_command) {
        delta += camera_speed * camera->front();
    }
    if ((uint32_t)EditorCommand::camera_back & m_editor_command) {
        delta += -camera_speed * camera->front();
    }
    if ((uint32_t)EditorCommand::camera_right & m_editor_command) {
        delta += camera_speed * camera->right();
    }
    if ((uint32_t)EditorCommand::camera_left & m_editor_command) {
        delta += -camera_speed * camera->right();
    }
    if ((uint32_t)EditorCommand::camera_up & m_editor_command) {
        delta += camera_speed * camera->up();
    }
    if ((uint32_t)EditorCommand::camera_down & m_editor_command) {
        delta += -camera_speed * camera->up();
    }

    camera->move(delta);
}

}  // namespace Vain