#pragma once

#include <memory>

#include "editor_ui.h"

namespace Vain {

class VainEngine;

class VainEditor {
  public:
    VainEditor() = default;
    ~VainEditor();

    void initialize(VainEngine *engine);
    void clear();

    void run();

  private:
    VainEngine *m_engine_runtime{};
    std::unique_ptr<EditorUI> m_editor_ui;
};

}  // namespace Vain