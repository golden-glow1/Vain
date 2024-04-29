#include "editor_ui.h"

#include <core/base/macro.h>
#include <function/global/global_context.h>
#include <function/render/render_system.h>
#include <function/render/window_system.h>
#include <imgui.h>
#include <imgui_internal.h>
#include <resource/config_manager.h>

#include "editor_global_context.h"

namespace Vain {

std::vector<std::pair<std::string, bool>> g_editor_node_state_array{};

int g_node_depth = -1;

void drawVecControl(
    const std::string &label,
    glm::vec3 &values,
    float reset_value = 0.0f,
    float column_width = 100.0f
) {
    ImGui::PushID(label.c_str());
    ImGui::Columns(2);

    ImGui::SetColumnWidth(0, column_width);
    ImGui::Text("%s", label.c_str());
    ImGui::NextColumn();

    ImGui::PushMultiItemsWidths(3, ImGui::CalcItemWidth());
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2{0, 0});

    float line_height = GImGui->Font->FontSize + GImGui->Style.FramePadding.y * 2.0f;
    ImVec2 button_size = {line_height + 3.0f, line_height};

    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{0.8f, 0.1f, 0.15f, 1.0f});
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4{0.9f, 0.2f, 0.2f, 1.0f});
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4{0.8f, 0.1f, 0.15f, 1.0f});
    if (ImGui::Button("X", button_size)) {
        values.x = reset_value;
    }
    ImGui::PopStyleColor(3);

    ImGui::SameLine();
    ImGui::DragFloat("##X", &values.x, 0.1f, 0.0f, 0.0f, "%.2f");
    ImGui::PopItemWidth();
    ImGui::SameLine();

    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{0.2f, 0.45f, 0.2f, 1.0f});
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4{0.3f, 0.55f, 0.3f, 1.0f});
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4{0.2f, 0.45f, 0.2f, 1.0f});
    if (ImGui::Button("Y", button_size)) {
        values.y = reset_value;
    }
    ImGui::PopStyleColor(3);

    ImGui::SameLine();
    ImGui::DragFloat("##Y", &values.y, 0.1f, 0.0f, 0.0f, "%.2f");
    ImGui::PopItemWidth();
    ImGui::SameLine();

    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{0.1f, 0.25f, 0.8f, 1.0f});
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4{0.2f, 0.35f, 0.9f, 1.0f});
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4{0.1f, 0.25f, 0.8f, 1.0f});
    if (ImGui::Button("Z", button_size)) {
        values.z = reset_value;
    }
    ImGui::PopStyleColor(3);

    ImGui::SameLine();
    ImGui::DragFloat("##Z", &values.z, 0.1f, 0.0f, 0.0f, "%.2f");
    ImGui::PopItemWidth();

    ImGui::PopStyleVar();

    ImGui::Columns(1);
    ImGui::PopID();
}

EditorUI::EditorUI() {
    const auto &asset_folder = g_runtime_global_context.config_manager->getAssetFolder();

    m_editor_ui_creator["TreeNodePush"] =
        [this](const std::string &name, void *value_ptr) -> void {
        static ImGuiTableFlags flags =
            ImGuiTableFlags_Resizable | ImGuiTableFlags_NoSavedSettings;
        bool node_state = false;
        g_node_depth++;
        if (g_node_depth > 0) {
            if (g_editor_node_state_array[g_node_depth - 1].second) {
                node_state =
                    ImGui::TreeNodeEx(name.c_str(), ImGuiTreeNodeFlags_SpanFullWidth);
            } else {
                g_editor_node_state_array.emplace_back(std::pair(name.c_str(), node_state)
                );
                return;
            }
        } else {
            node_state =
                ImGui::TreeNodeEx(name.c_str(), ImGuiTreeNodeFlags_SpanFullWidth);
        }
        g_editor_node_state_array.emplace_back(std::pair(name.c_str(), node_state));
    };

    m_editor_ui_creator["TreeNodePop"] =
        [this](const std::string &name, void *value_ptr) -> void {
        if (g_editor_node_state_array[g_node_depth].second) {
            ImGui::TreePop();
        }
        g_editor_node_state_array.pop_back();
        g_node_depth--;
    };

    m_editor_ui_creator["Transform"] = [this](const std::string &name, void *value_ptr) {
        if (!g_editor_node_state_array[g_node_depth].second) {
            return;
        }

        Transform *trans_ptr = static_cast<Transform *>(value_ptr);

        glm::vec3 euler = glm::degrees(glm::eulerAngles(trans_ptr->rotation));

        drawVecControl("Translation", trans_ptr->translation);
        drawVecControl("Rotation", euler);
        drawVecControl("Scale", trans_ptr->scale);

        trans_ptr->rotation = glm::quat(glm::radians(euler));
    };

    m_editor_ui_creator["bool"] =
        [this](const std::string &name, void *value_ptr) -> void {
        if (g_node_depth == -1) {
            std::string label = "##" + name;
            ImGui::Text("%s", name.c_str());
            ImGui::SameLine();
            ImGui::Checkbox(label.c_str(), static_cast<bool *>(value_ptr));
        } else {
            if (g_editor_node_state_array[g_node_depth].second) {
                std::string full_label = "##" + getLeafUINodeParentLabel() + name;
                ImGui::Text("%s", name.c_str());
                ImGui::Checkbox(full_label.c_str(), static_cast<bool *>(value_ptr));
            }
        }
    };

    m_editor_ui_creator["int"] =
        [this](const std::string &name, void *value_ptr) -> void {
        if (g_node_depth == -1) {
            std::string label = "##" + name;
            ImGui::Text("%s", name.c_str());
            ImGui::SameLine();
            ImGui::InputInt(label.c_str(), static_cast<int *>(value_ptr));
        } else {
            if (g_editor_node_state_array[g_node_depth].second) {
                std::string full_label = "##" + getLeafUINodeParentLabel() + name;
                ImGui::Text("%s", (name + ":").c_str());
                ImGui::InputInt(full_label.c_str(), static_cast<int *>(value_ptr));
            }
        }
    };

    m_editor_ui_creator["float"] =
        [this](const std::string &name, void *value_ptr) -> void {
        if (g_node_depth == -1) {
            std::string label = "##" + name;
            ImGui::Text("%s", name.c_str());
            ImGui::SameLine();
            ImGui::InputFloat(label.c_str(), static_cast<float *>(value_ptr));
        } else {
            if (g_editor_node_state_array[g_node_depth].second) {
                std::string full_label = "##" + getLeafUINodeParentLabel() + name;
                ImGui::Text("%s", (name + ":").c_str());
                ImGui::InputFloat(full_label.c_str(), static_cast<float *>(value_ptr));
            }
        }
    };

    m_editor_ui_creator["vec3"] =
        [this](const std::string &name, void *value_ptr) -> void {
        glm::vec3 *vec_ptr = static_cast<glm::vec3 *>(value_ptr);
        float val[3] = {vec_ptr->x, vec_ptr->y, vec_ptr->z};
        if (g_node_depth == -1) {
            std::string label = "##" + name;
            ImGui::Text("%s", name.c_str());
            ImGui::SameLine();
            ImGui::DragFloat3(label.c_str(), val);
        } else {
            if (g_editor_node_state_array[g_node_depth].second) {
                std::string full_label = "##" + getLeafUINodeParentLabel() + name;
                ImGui::Text("%s", (name + ":").c_str());
                ImGui::DragFloat3(full_label.c_str(), val);
            }
        }
        vec_ptr->x = val[0];
        vec_ptr->y = val[1];
        vec_ptr->z = val[2];
    };

    m_editor_ui_creator["quat"] =
        [this](const std::string &name, void *value_ptr) -> void {
        glm::quat *quat_ptr = static_cast<glm::quat *>(value_ptr);
        float val[4] = {quat_ptr->x, quat_ptr->y, quat_ptr->z, quat_ptr->w};
        if (g_node_depth == -1) {
            std::string label = "##" + name;
            ImGui::Text("%s", name.c_str());
            ImGui::SameLine();
            ImGui::DragFloat4(label.c_str(), val);
        } else {
            if (g_editor_node_state_array[g_node_depth].second) {
                std::string full_label = "##" + getLeafUINodeParentLabel() + name;
                ImGui::Text("%s", (name + ":").c_str());
                ImGui::DragFloat4(full_label.c_str(), val);
            }
        }
        quat_ptr->x = val[0];
        quat_ptr->y = val[1];
        quat_ptr->z = val[2];
        quat_ptr->w = val[3];
    };

    m_editor_ui_creator["std::string"] =
        [this, &asset_folder](const std::string &name, void *value_ptr) -> void {
        if (g_node_depth == -1) {
            std::string label = "##" + name;
            ImGui::Text("%s", name.c_str());
            ImGui::SameLine();
            ImGui::Text("%s", (*static_cast<std::string *>(value_ptr)).c_str());
        } else {
            if (g_editor_node_state_array[g_node_depth].second) {
                std::string full_label = "##" + getLeafUINodeParentLabel() + name;
                ImGui::Text("%s", (name + ":").c_str());
                std::string value_str = *static_cast<std::string *>(value_ptr);
                if (value_str.find_first_of('/') != std::string::npos) {
                    std::filesystem::path value_path(value_str);
                    if (value_path.is_absolute()) {
                        value_path = value_path.lexically_relative(asset_folder);
                    }
                    value_str = value_path.generic_string();
                    if (value_str.size() >= 2 && value_str[0] == '.' &&
                        value_str[1] == '.') {
                        value_str.clear();
                    }
                }
                ImGui::Text("%s", value_str.c_str());
            }
        }
    };
}

EditorUI::~EditorUI() { clear(); }

void EditorUI::initialize(WindowUIInitInfo init_info) {
    init_info.render_system->initializeUIRenderBackend(this);
}

void EditorUI::preRender() {
    showEditorFileContentWindow(&m_file_content_window_open);
    showObjectDetailWindow(&m_object_detail_window);
}

void EditorUI::clear() {}

std::string EditorUI::getLeafUINodeParentLabel() {
    std::string parent_label{};
    for (const auto &[label, _] : g_editor_node_state_array) {
        parent_label += label + "::";
    }
    return parent_label;
}

void EditorUI::showEditorFileContentWindow(bool *p_open) {
    ImGuiWindowFlags window_flags = ImGuiWindowFlags_None;

    if (!*p_open) {
        return;
    }

    if (!ImGui::Begin("File Content", p_open, window_flags)) {
        ImGui::End();
        return;
    }

    static ImGuiTableFlags flags = ImGuiTableFlags_BordersV |
                                   ImGuiTableFlags_BordersOuterH |
                                   ImGuiTableFlags_Resizable | ImGuiTableFlags_RowBg |
                                   ImGuiTableFlags_NoBordersInBody;

    if (ImGui::BeginTable("File Content", 2, flags)) {
        ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_NoHide);
        ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableHeadersRow();

        auto current_time = std::chrono::steady_clock::now();
        if (current_time - m_last_file_tree_update > std::chrono::seconds(1)) {
            m_editor_file_service.buildEngineFileTree();
            m_last_file_tree_update = current_time;
        }
        m_last_file_tree_update = current_time;

        EditorFileNode *editor_root_node = m_editor_file_service.getEditorRootNode();
        buildEditorFileAssetsUITree(editor_root_node);
        ImGui::EndTable();
    }

    ImGui::End();
}

void EditorUI::showObjectDetailWindow(bool *p_open) {
    ImGuiWindowFlags window_flags = ImGuiWindowFlags_None;

    if (!*p_open) {
        return;
    }

    if (!ImGui::Begin("Object Details", p_open, window_flags)) {
        ImGui::End();
        return;
    }

    ImGui::End();
}

void EditorUI::buildEditorFileAssetsUITree(EditorFileNode *node) {
    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    const bool is_folder = (node->child_nodes.size() > 0);
    if (is_folder) {
        bool open =
            ImGui::TreeNodeEx(node->file_name.c_str(), ImGuiTreeNodeFlags_SpanFullWidth);
        ImGui::TableNextColumn();
        ImGui::SetNextItemWidth(100.0f);
        ImGui::TextUnformatted(node->file_type.c_str());
        if (open) {
            for (const auto &child : node->child_nodes) {
                buildEditorFileAssetsUITree(child.get());
            }
            ImGui::TreePop();
        }
    } else {
        ImGui::TreeNodeEx(
            node->file_name.c_str(),
            ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen |
                ImGuiTreeNodeFlags_SpanFullWidth
        );

        if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen()) {
            onFileContentItemClicked(node);
        }
        ImGui::TableNextColumn();
        ImGui::SetNextItemWidth(100.0f);
        ImGui::TextUnformatted(node->file_type.c_str());
    }
}

void EditorUI::onFileContentItemClicked(EditorFileNode *node) {
    if (node->file_type != "gltf") {
        return;
    }

    g_editor_global_context.render_system->spawnObject(node->file_path);
    VAIN_INFO("Loaded object: {}", node->file_path);
}

}  // namespace Vain