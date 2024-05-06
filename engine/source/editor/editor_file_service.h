#pragma once

#include <memory>
#include <string>
#include <vector>

namespace Vain {

struct EditorFileNode {
    std::string file_name{};
    std::string file_type{};
    std::string file_path{};

    std::vector<std::shared_ptr<EditorFileNode>> child_nodes{};

    EditorFileNode() = default;
    EditorFileNode(
        const std::string &name, const std::string &type, const std::string &path
    )
        : file_name(name), file_type(type), file_path(path) {}
};

class EditorFileService {
  public:
    EditorFileNode *getEditorRootNode() const { return m_root_node.get(); }

    void buildEngineFileTree();

  private:
    std::shared_ptr<EditorFileNode> m_root_node{};
};

}  // namespace Vain