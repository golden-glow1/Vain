#include "editor.h"

#include <function/global/global_context.h>

#include "editor_global_context.h"

namespace Vain {

VainEditor::~VainEditor() { clear(); }

void VainEditor::initialize(VainEngine *engine) {
    m_engine = engine;

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

}  // namespace Vain