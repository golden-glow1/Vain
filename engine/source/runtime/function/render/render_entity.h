#pragma once

#include "core/math/aabb.h"

namespace Vain {

// combine mesh and PBR material together
class RenderEntity {
  public:
    size_t entity_id{0};
    glm::mat4 model_matrix{1.0};

    // mesh
    size_t mesh_asset_id{0};
    AxisAlignedBoundingBox aabb{};

    // material
    size_t material_asset_id{0};
    bool blend{false};
    bool double_sided{false};
    glm::vec4 base_color_factor{1.0f, 1.0f, 1.0f, 1.0f};
    float metallic_factor{1.0f};
    float roughness_factor{1.0f};
    float normal_scale{1.0f};
    float occlusion_strength{1.0f};
    glm::vec3 emissive_factor{0.0f, 0.0f, 0.0f};
};

}  // namespace Vain