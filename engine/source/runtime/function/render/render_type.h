#pragma once

#include <vulkan/vulkan.h>

#include <array>
#include <string>

#include "core/base/hash.h"
#include "resource/asset_type.h"

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/hash.hpp>

namespace Vain {

static const uint32_t k_point_light_shadow_map_dimension = 2048;
static const uint32_t k_directional_light_shadow_map_dimension = 4096;

static uint32_t const k_mesh_per_drawcall_max_instance_count = 64;
static uint32_t const k_max_point_light_count = 15;

struct MeshVertex {
    glm::vec3 position{};
    glm::vec3 normal{};
    glm::vec3 tangent{};
    glm::vec2 texcoord{};

    bool operator==(const MeshVertex &rhs) const {
        return position == rhs.position && normal == rhs.normal &&
               tangent == rhs.tangent && texcoord == rhs.texcoord;
    }

    static std::array<VkVertexInputBindingDescription, 1> getBindingDescriptions();
    static std::array<VkVertexInputAttributeDescription, 4> getAttributeDescriptions();
};

struct DirectionalLight {
    glm::vec3 direction{};
    float _padding_direction{};
    glm::vec3 color{};
    float _padding_color{};
};

struct PointLight {
    glm::vec3 position{};
    float radius{};
    glm::vec3 intensity;
    float _padding_intensity{};
};

struct MeshPerFrameStorageBufferObject {
    glm::mat4 proj_view_matrix{};
    glm::vec3 camera_position{};
    float _padding_camera_position{};
    glm::vec3 ambient_light{};
    float _padding_ambient_light{};
    uint32_t point_light_num{};
    uint32_t _padding_point_light_num_1{};
    uint32_t _padding_point_light_num_2{};
    uint32_t _padding_point_light_num_3{};
    PointLight scene_point_lights[k_max_point_light_count]{};
    DirectionalLight scene_directional_light{};
    glm::mat4 directional_light_proj_view{};
};

struct MeshInstance {
    glm::mat4 model_matrix{};
};

struct MeshPerDrawcallStorageBufferObject {
    MeshInstance mesh_instances[k_mesh_per_drawcall_max_instance_count]{};
};

struct MeshPerMaterialUniformBufferObject {
    glm::vec4 base_color_factor{};

    float metallic_factor{};
    float roughness_factor{};
    float normal_scale{};
    float occlusion_strength{};

    glm::vec3 emissive_factor{};
    uint32_t is_blend{};
    uint32_t is_double_sided{};
};

struct DirectionalLightShadowPerFrameStorageBufferObject {
    glm::mat4 light_proj_view{};
};

struct PointLightShadowPerFrameStorageBufferObject {
    uint32_t point_light_num{};
    uint32_t _padding_point_light_num_1{};
    uint32_t _padding_point_light_num_2{};
    uint32_t _padding_point_light_num_3{};
    glm::vec4 point_lights_position_and_radius[k_max_point_light_count]{};
};

}  // namespace Vain

namespace std {

template <>
struct hash<Vain::MeshVertex> {
    size_t operator()(const Vain::MeshVertex &vertex) const {
        size_t hash = 0;
        hash_combine(
            hash, vertex.position, vertex.normal, vertex.tangent, vertex.texcoord
        );
        return hash;
    }
};

}  // namespace std