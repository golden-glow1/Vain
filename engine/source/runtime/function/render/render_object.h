#pragma once

#include <functional>
#include <string>

#include "core/math/transform.h"
#include "function/framework/object_id.h"

namespace Vain {

struct GameObjectMeshDesc {
    std::string mesh_file{};
};

struct GameObjectMaterialDesc {
    std::string base_color_texture_file{};
    std::string normal_texture_file{};
    std::string metallic_texture_file{};
    std::string roughness_texture_file{};
    std::string occlusion_texture_file{};
    std::string emissive_texture_file{};
    bool with_texture{false};
};

struct GameObjectDesc {
    GObjectID go_id{k_invalid_go_id};
    GameObjectMeshDesc mesh_desc{};
    GameObjectMaterialDesc material_desc{};
    Transform transform{};
};

}  // namespace Vain
