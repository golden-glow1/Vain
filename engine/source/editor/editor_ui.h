#pragma once

#include <function/render/ui/window_ui.h>

#include <chrono>
#include <functional>
#include <string>
#include <unordered_map>

#include "editor_file_service.h"

namespace Vain {

class EditorUI : public WindowUI {
  public:
    EditorUI();
    ~EditorUI();

    virtual void initialize(WindowUIInitInfo init_info) override;
    virtual void preRender() override;

    void clear();

  private:
    using UICreator = std::function<void(std::string, void *)>;

    std::unordered_map<std::string, UICreator> m_editor_ui_creator{};
    EditorFileService m_editor_file_service{};
    std::chrono::time_point<std::chrono::steady_clock> m_last_file_tree_update{};

    bool m_file_content_window_open = true;

    std::string getLeafUINodeParentLabel();

    void showEditorFileContentWindow(bool *p_open);

    void buildEditorFileAssetsUITree(EditorFileNode *node);
};

}  // namespace Vain