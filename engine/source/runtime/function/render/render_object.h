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

struct GameObjectPartDesc {
    GameObjectMeshDesc mesh_desc{};
    GameObjectMaterialDesc material_desc{};
    Transform transform{};
};

constexpr size_t k_invalid_part_id = std::numeric_limits<size_t>::max();

struct GObjectPartID {
    GObjectID go_id{k_invalid_go_id};
    size_t part_id{k_invalid_part_id};

    operator bool() const {
        return go_id != k_invalid_go_id && part_id != k_invalid_part_id;
    }
};

class GameObjectDesc {
  public:
    GameObjectDesc(GObjectID go_id, const std::vector<GameObjectPartDesc> &object_parts)
        : go_id(go_id), object_parts(object_parts) {}

    GObjectID go_id{k_invalid_go_id};
    std::vector<GameObjectPartDesc> object_parts{};
};

}  // namespace Vain

namespace std {

template <>
struct hash<Vain::GObjectPartID> {
    size_t operator()(const Vain::GObjectPartID &id) const {
        return id.go_id ^ (id.part_id << 1);
    }
};

}  // namespace std