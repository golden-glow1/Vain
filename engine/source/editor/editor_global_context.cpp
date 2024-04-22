#include "editor_global_context.h"

#include <engine.h>
#include <function/render/render_system.h>
#include <function/render/window_system.h>

#include "editor_input_manager.h"
#include "editor_scene_manager.h"

namespace Vain {

EditorGlobalContext g_editor_global_context{};

EditorGlobalContext::~EditorGlobalContext() { clear(); }

void EditorGlobalContext::initialize(const EditorGlobalContextInitInfo &init_info) {
    render_system = init_info.render_system;
    window_system = init_info.window_system;
    engine_runtime = init_info.engine_runtime;

    input_manager = std::make_unique<EditorInputManager>();
    input_manager->initialize();

    scene_manager = std::make_unique<EditorSceneManager>();
    scene_manager->initialize();
}

void EditorGlobalContext::clear() {
    scene_manager.reset();
    input_manager.reset();
}

}  // namespace Vain