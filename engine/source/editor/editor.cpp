#include "editor.h"

#include <engine.h>
#include <function/global/global_context.h>
#include <function/render/window_system.h>

#include "editor_global_context.h"
#include "editor_input_manager.h"

namespace Vain {

VainEditor::~VainEditor() { clear(); }

void VainEditor::initialize(VainEngine *engine) {
    m_engine_runtime = engine;

    EditorGlobalContextInitInfo init_info{
        g_runtime_global_context.render_system.get(),
        g_runtime_global_context.window_system.get(),
        engine
    };
    g_editor_global_context.initialize(init_info);

    m_editor_ui = std::make_unique<EditorUI>();
    WindowUIInitInfo ui_init_info{
        g_runtime_global_context.render_system.get(),
        g_runtime_global_context.window_system.get()
    };
    m_editor_ui->initialize(ui_init_info);
}

void VainEditor::clear() { m_editor_ui.reset(); }

void VainEditor::run() {
    while (!g_editor_global_context.window_system->shouldClose()) {
        float delta_time = m_engine_runtime->calculateDeltaTime();
        g_editor_global_context.input_manager->tick(delta_time);
        m_engine_runtime->tickOneFrame(delta_time);
    }
}

}  // namespace Vain