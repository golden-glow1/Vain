#include "editor_file_service.h"

#include <function/global/global_context.h>
#include <resource/config_manager.h>

#include <filesystem>
#include <queue>

namespace Vain {

void EditorFileService::buildEngineFileTree() {
    auto asset_folder = g_runtime_global_context.config_manager->getAssetFolder();
    auto root_folder = g_runtime_global_context.config_manager->getRootFolder();

    std::queue<std::shared_ptr<EditorFileNode>> queue;

    m_root_node = std::make_shared<EditorFileNode>(
        asset_folder.filename().generic_string(),
        "Folder",
        asset_folder.lexically_relative(root_folder).generic_string()
    );
    queue.push(m_root_node);

    while (!queue.empty()) {
        auto current = queue.front();
        queue.pop();
        auto iter = std::filesystem::directory_iterator(root_folder / current->file_path);
        for (const auto &entry : iter) {
            auto child = std::make_shared<EditorFileNode>();
            child->file_name = entry.path().filename().generic_string();
            child->file_path =
                entry.path().lexically_relative(root_folder).generic_string();

            if (entry.is_directory()) {
                child->file_type = "Folder";
                queue.push(child);
            } else {
                std::string type = entry.path().extension().generic_string();
                if (type != "") {
                    child->file_type = type.substr(1);
                }
            }
            current->child_nodes.emplace_back(child);
        }
    }
}

}  // namespace Vain