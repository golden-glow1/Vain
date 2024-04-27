#include "editor_file_service.h"

#include <function/global/global_context.h>
#include <resource/config_manager.h>

#include <filesystem>

namespace Vain {

void EditorFileService::buildEngineFileTree() {
    auto asset_folder = g_runtime_global_context.config_manager->getAssetFolder();
    auto root_folder = g_runtime_global_context.config_manager->getRootFolder();
    std::string asset_folder_name;
    if (asset_folder.has_filename()) {
        asset_folder_name = asset_folder.filename().generic_string();
    } else {
        asset_folder_name = asset_folder.parent_path().filename().generic_string();
    }

    std::vector<std::filesystem::path> file_paths{};
    auto iter = std::filesystem::recursive_directory_iterator{asset_folder};
    for (auto const &directory_entry : iter) {
        if (directory_entry.is_regular_file()) {
            file_paths.push_back(directory_entry);
        }
    }

    std::vector<std::vector<std::string>> all_file_segments;
    for (const auto &path : file_paths) {
        auto relative_path = path.lexically_relative(asset_folder);
        all_file_segments.emplace_back();
        for (auto it = relative_path.begin(); it != relative_path.end(); ++it) {
            all_file_segments.back().push_back(it->generic_string());
        }
    }

    std::vector<std::shared_ptr<EditorFileNode>> node_array;

    m_file_node_array.clear();
    auto root_node =
        std::make_shared<EditorFileNode>(asset_folder_name, "Folder", "", -1);
    m_file_node_array.push_back(root_node);

    int all_file_segments_count = all_file_segments.size();
    for (int file_index = 0; file_index < all_file_segments_count; ++file_index) {
        int depth = 0;
        node_array.clear();
        node_array.push_back(root_node);

        int segment_count = all_file_segments[file_index].size();
        for (int segment_index = 0; segment_index < segment_count; ++segment_index) {
            auto file_node = std::make_shared<EditorFileNode>();

            file_node->file_name = all_file_segments[file_index][segment_index];
            file_node->node_depth = depth;
            if (depth < segment_count - 1) {
                file_node->file_type = "Folder";
            } else {
                std::string type = file_paths[file_index].extension().generic_string();
                if (type.length() == 0) {
                    continue;
                }
                if (type == ".json") {
                    type = file_paths[file_index].stem().extension().generic_string();
                }
                file_node->file_type = type.substr(1);
                file_node->file_path = file_paths[file_index]
                                           .lexically_relative(root_folder)
                                           .generic_string();
            }

            node_array.push_back(file_node);

            bool node_exists = findNodeInArray(file_node.get());
            if (!node_exists) {
                m_file_node_array.push_back(file_node);
            }
            auto parent_node = findNodeInArray(node_array[depth].get());
            if (parent_node && !node_exists) {
                parent_node->child_nodes.push_back(file_node);
            }
            depth += 1;
        }
    }
}

EditorFileNode *EditorFileService::findNodeInArray(EditorFileNode *file_node) {
    for (const auto &array_node : m_file_node_array) {
        if (array_node->file_name == file_node->file_name &&
            array_node->node_depth == file_node->node_depth) {
            return array_node.get();
        }
    }
    return nullptr;
}

}  // namespace Vain