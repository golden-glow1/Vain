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

  private:
    VainEngine *m_engine{};
    std::unique_ptr<EditorUI> m_editor_ui;
};

}  // namespace Vain