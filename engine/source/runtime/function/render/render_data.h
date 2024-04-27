#pragma once

#include <memory>
#include <string>
#include <vector>

#include "core/math/aabb.h"
#include "render_type.h"

namespace Vain {

struct IBLData {
    void *brdfLUT_texture_image_pixels{};
    uint32_t brdfLUT_texture_image_width{};
    uint32_t brdfLUT_texture_image_height{};
    VkFormat brdfLUT_texture_image_format{};
    std::array<void *, 6> irradiance_texture_image_pixels{};
    uint32_t irradiance_texture_image_width{};
    uint32_t irradiance_texture_image_height{};
    VkFormat irradiance_texture_image_format{};
    std::array<void *, 6> specular_texture_image_pixels{};
    uint32_t specular_texture_image_width{};
    uint32_t specular_texture_image_height{};
    VkFormat specular_texture_image_format{};

    ~IBLData();
};

struct TextureData {
    uint32_t width{};
    uint32_t height{};
    uint32_t depth{};
    uint32_t mip_levels{};
    uint32_t array_layers{};
    void *pixels{};

    VkFormat format{};

    ~TextureData();
};

struct MeshData {
    std::vector<MeshVertex> vertices{};
    std::vector<uint32_t> indices{};
};

struct PBRMaterialData {
    std::shared_ptr<TextureData> base_color_texture{};
    std::shared_ptr<TextureData> metallic_texture{};
    std::shared_ptr<TextureData> roughness_texture{};
    std::shared_ptr<TextureData> normal_texture{};
    std::shared_ptr<TextureData> occlusion_texture{};
    std::shared_ptr<TextureData> emissive_texture{};
};

std::shared_ptr<TextureData> loadTextureHDR(
    const std::string &file, int desired_channels = 4
);

std::shared_ptr<TextureData> loadTexture(const std::string &file, bool is_srgb = false);

PBRMaterialData loadPBRMaterial(const PBRMaterialDesc &desc);

MeshData loadMeshData(const std::string &file, AxisAlignedBoundingBox &aabb);

}  // namespace Vain