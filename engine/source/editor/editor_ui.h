#pragma once

#include <function/render/ui/window_ui.h>

namespace Vain {

class EditorUI : public WindowUI {
  public:
    EditorUI() = default;
    ~EditorUI();

    virtual void initialize(WindowUIInitInfo init_info) override;
    virtual void preRender() override;

    void clear();
};

}  // namespace Vain