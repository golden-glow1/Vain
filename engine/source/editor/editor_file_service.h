#pragma once

#include <memory>
#include <string>
#include <vector>

namespace Vain {

struct EditorFileNode {
    std::string file_name{};
    std::string file_type{};
    std::string file_path{};

    int node_depth{};
    std::vector<std::shared_ptr<EditorFileNode>> child_nodes{};

    EditorFileNode() = default;
    EditorFileNode(
        const std::string &name,
        const std::string &type,
        const std::string &path,
        int depth
    )
        : file_name(name), file_type(type), file_path(path), node_depth(depth) {}
};

class EditorFileService {
  public:
    EditorFileNode *getEditorRootNode() {
        return m_file_node_array.empty() ? nullptr : m_file_node_array[0].get();
    }

    void buildEngineFileTree();

  private:
    std::vector<std::shared_ptr<EditorFileNode>> m_file_node_array;

    EditorFileNode *findNodeInArray(EditorFileNode *file_node);
};

}  // namespace Vain