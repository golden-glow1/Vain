#include "render_data.h"

#include <assert.h>

#include <assimp/Importer.hpp>
#include <unordered_map>

#include "core/base/macro.h"
#include "function/global/global_context.h"
#include "resource/asset_manager.h"

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

namespace Vain {

IBLData::~IBLData() {
    if (brdfLUT_texture_image_pixels) {
        stbi_image_free(brdfLUT_texture_image_pixels);
        brdfLUT_texture_image_pixels = nullptr;
    }

    for (auto pixels : irradiance_texture_image_pixels) {
        if (pixels) {
            stbi_image_free(pixels);
            pixels = nullptr;
        }
    }

    for (auto pixels : specular_texture_image_pixels) {
        if (pixels) {
            stbi_image_free(pixels);
            pixels = nullptr;
        }
    }
}

TextureData::~TextureData() {
    if (pixels) {
        stbi_image_free(pixels);
        pixels = nullptr;
    }
}

std::shared_ptr<TextureData> loadTextureHDR(
    const std::string &file, int desired_channels
) {
    AssetManager *asset_manager = g_runtime_global_context.asset_manager.get();
    std::shared_ptr<TextureData> texture = std::make_shared<TextureData>();

    int iw, ih, n;
    texture->pixels = stbi_loadf(
        asset_manager->getFullPath(file).generic_string().c_str(),
        &iw,
        &ih,
        &n,
        desired_channels
    );

    if (!texture->pixels) {
        return nullptr;
    }

    switch (desired_channels) {
    case 2:
        texture->format = VK_FORMAT_R32G32_SFLOAT;
        break;
    case 4:
        texture->format = VK_FORMAT_R32G32B32A32_SFLOAT;
        break;
    default:
        VAIN_ERROR("unsupported channels number");
        return nullptr;
    }

    texture->width = iw;
    texture->height = ih;
    texture->depth = 1;
    texture->array_layers = 1;
    texture->mip_levels = 1;

    return texture;
}

std::shared_ptr<TextureData> loadTexture(const std::string &file, bool is_srgb) {
    AssetManager *asset_manager = g_runtime_global_context.asset_manager.get();
    std::shared_ptr<TextureData> texture = std::make_shared<TextureData>();

    int iw, ih, n;
    texture->pixels = stbi_load(
        asset_manager->getFullPath(file).generic_string().c_str(), &iw, &ih, &n, 4
    );

    if (!texture->pixels) {
        return nullptr;
    }

    texture->width = iw;
    texture->height = ih;
    texture->depth = 1;
    texture->array_layers = 1;
    texture->mip_levels = 1;

    texture->format = (is_srgb) ? VK_FORMAT_R8G8B8A8_SRGB : VK_FORMAT_R8G8B8A8_UNORM;

    return texture;
}

PBRMaterialData loadPBRMaterial(const PBRMaterialDesc &desc) {
    PBRMaterialData data{};

    data.base_color_texture = loadTexture(desc.base_color_file);
    data.normal_texture = loadTexture(desc.normal_file);
    data.metallic_texture = loadTexture(desc.metallic_file);
    data.roughness_texture = loadTexture(desc.roughness_file);
    data.occlusion_texture = loadTexture(desc.occlusion_file);
    data.emissive_texture = loadTexture(desc.emissive_file);

    return data;
}

MeshData loadMeshData(const std::string &file, AxisAlignedBoundingBox &aabb) {
    AssetManager *asset_manager = g_runtime_global_context.asset_manager.get();
    MeshData mesh_data{};

    return mesh_data;
}

}  // namespace Vain