#include "editor_ui.h"

#include <function/render/render_system.h>
#include <function/render/window_system.h>
#include <imgui.h>

namespace Vain {

EditorUI::~EditorUI() { clear(); }

void EditorUI::initialize(WindowUIInitInfo init_info) {
    init_info.render_system->initializeUIRenderBackend(this);
}

void EditorUI::preRender() { ImGui::ShowDemoWindow(); }

void EditorUI::clear() {}

}  // namespace Vain