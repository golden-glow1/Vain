#pragma once

#include <functional>
#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>
#include <string>

#include "core/base/hash.h"

namespace Vain {

struct Color {
    float r{};
    float g{};
    float b{};
};

struct SkyBoxDesc {
    std::string positive_x_map{};
    std::string negative_x_map{};
    std::string positive_y_map{};
    std::string negative_y_map{};
    std::string positive_z_map{};
    std::string negative_z_map{};
};

struct IBLDesc {
    std::string brdf_map{};
    SkyBoxDesc skybox_irradiance_map{};
    SkyBoxDesc skybox_specular_map{};
};

struct MeshDesc {
    std::string mesh_file{};

    bool operator==(const MeshDesc &rhs) const { return mesh_file == rhs.mesh_file; }
};

struct PBRMaterialDesc {
    std::string base_color_file{};
    std::string normal_file{};
    std::string metallic_file{};
    std::string roughness_file{};
    std::string occlusion_file{};
    std::string emissive_file{};

    bool operator==(const PBRMaterialDesc &rhs) const {
        return base_color_file == rhs.base_color_file && normal_file == rhs.normal_file &&
               metallic_file == rhs.metallic_file &&
               roughness_file == rhs.roughness_file &&
               occlusion_file == rhs.occlusion_file && emissive_file == rhs.emissive_file;
    }
};

struct DirectionalLightDesc {
    glm::vec3 direction{};
    Color color{};
};

struct PointLightDesc {
    glm::vec3 position{};
    glm::vec3 flux{};

    float getRadius() const {
        constexpr float INTENSITY_CUTOFF = 1.0f;
        constexpr float ATTENTUATION_CUTOFF = 0.05f;

        glm::vec3 intensity = flux * 0.25f / glm::pi<float>();
        float max_intensity = std::max({intensity.r, intensity.g, intensity.b});
        float attenuation =
            std::max(INTENSITY_CUTOFF, ATTENTUATION_CUTOFF * max_intensity) /
            max_intensity;
        return 1.0 / sqrt(attenuation);
    }
};

struct CameraPose {
    glm::vec3 position{};
    glm::vec3 target{};
    glm::vec3 up{};
};

struct CameraConfig {
    CameraPose pose{};
    float z_far{};
    float z_near{};
    glm::vec2 aspect{};
};

struct SceneGlobalDesc {
    SkyBoxDesc skybox_irradiance_map{};
    SkyBoxDesc skybox_specular_map{};
    std::string brdf_map{};
    Color sky_color{};
    Color ambient_light{};
    CameraConfig camera_config{};
    DirectionalLightDesc directional_light{};
};

}  // namespace Vain

namespace std {

template <>
struct hash<Vain::MeshDesc> {
    size_t operator()(const Vain::MeshDesc &desc) const {
        return hash<std::string>{}(desc.mesh_file);
    }
};

template <>
struct hash<Vain::PBRMaterialDesc> {
    size_t operator()(const Vain::PBRMaterialDesc &desc) const {
        size_t hash = 0;
        hash_combine(
            hash,
            desc.base_color_file,
            desc.normal_file,
            desc.metallic_file,
            desc.roughness_file,
            desc.occlusion_file,
            desc.emissive_file
        );
        return hash;
    }
};

}  // namespace std