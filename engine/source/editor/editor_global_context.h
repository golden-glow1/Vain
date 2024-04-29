#pragma once

#include <memory>

namespace Vain {

class EditorInputManager;
class RenderSystem;
class VainEngine;
class WindowSystem;

struct EditorGlobalContextInitInfo {
    RenderSystem *render_system;
    WindowSystem *window_system;
    VainEngine *engine_runtime;
};

class EditorGlobalContext {
  public:
    RenderSystem *render_system{};
    WindowSystem *window_system{};
    VainEngine *engine_runtime{};

    std::unique_ptr<EditorInputManager> input_manager{};

    EditorGlobalContext() = default;
    ~EditorGlobalContext();

    void initialize(const EditorGlobalContextInitInfo &init_info);
    void clear();
};

extern EditorGlobalContext g_editor_global_context;

}  // namespace Vain