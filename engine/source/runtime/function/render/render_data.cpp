#include "render_data.h"

#include <assert.h>

#include <unordered_map>

#include "core/base/macro.h"
#include "function/global/global_context.h"
#include "resource/asset_manager.h"

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#define TINYOBJLOADER_IMPLEMENTATION
#define TINYOBJLOADER_USE_MAPBOX_EARCUT
#include <tiny_obj_loader.h>

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

MeshData loadMeshDataFromObjFile(const std::string &file, AxisAlignedBoundingBox &aabb) {
    AssetManager *asset_manager = g_runtime_global_context.asset_manager.get();
    MeshData mesh_data{};

    std::unordered_map<MeshVertex, uint16_t> unique_vertices{};

    tinyobj::ObjReader reader{};
    tinyobj::ObjReaderConfig reader_config{};
    reader_config.triangulate = true;
    reader_config.vertex_color = false;
    reader_config.triangulation_method = "earcut";

    bool ok = reader.ParseFromFile(
        asset_manager->getFullPath(file).generic_string(), reader_config
    );
    if (!ok && !reader.Error().empty()) {
        VAIN_ERROR("load {} failed, error: {}", file, reader.Error());
    }

    if (!reader.Warning().empty()) {
        VAIN_WARN("load {} warning, warning: {}", file, reader.Warning());
    }

    auto &attrib = reader.GetAttrib();
    auto &shapes = reader.GetShapes();

    for (auto &shape : shapes) {
        size_t index_offset = 0;
        for (size_t f = 0; f < shape.mesh.num_face_vertices.size(); f++) {
            // triangulate enabled
            size_t fv = shape.mesh.num_face_vertices[f];
            assert(fv == 3);

            bool with_normal = false;
            bool with_texcoord = false;

            glm::vec3 position[3]{};
            glm::vec3 normal[3]{};
            glm::vec2 texcoord[3]{};

            for (size_t v = 0; v < fv; ++v) {
                auto idx = shape.mesh.indices[index_offset + v];
                position[v].x = attrib.vertices[3 * idx.vertex_index + 0];
                position[v].y = attrib.vertices[3 * idx.vertex_index + 1];
                position[v].z = attrib.vertices[3 * idx.vertex_index + 2];

                if (idx.normal_index >= 0) {
                    with_normal = true;

                    normal[v].x = attrib.normals[3 * idx.normal_index + 0];
                    normal[v].y = attrib.normals[3 * idx.normal_index + 1];
                    normal[v].z = attrib.normals[3 * idx.normal_index + 2];
                }

                if (idx.texcoord_index >= 0) {
                    with_texcoord = true;

                    texcoord[v].x = attrib.texcoords[2 * idx.texcoord_index + 0];
                    texcoord[v].y = attrib.texcoords[2 * idx.texcoord_index + 1];
                }
            }
            index_offset += fv;

            if (!with_normal) {
                glm::vec3 edge1 = position[1] - position[0];
                glm::vec3 edge2 = position[2] - position[1];

                normal[0] = glm::normalize(glm::cross(edge1, edge2));
                normal[1] = normal[0];
                normal[2] = normal[0];
            }

            if (!with_texcoord) {
                texcoord[0] = {0.5, 0.5};
                texcoord[1] = {0.5, 0.5};
                texcoord[2] = {0.5, 0.5};
            }

            glm::vec3 tangent{1, 0, 0};
            {
                glm::vec3 edge1 = position[1] - position[0];
                glm::vec3 edge2 = position[2] - position[1];
                glm::vec2 delta_uv1 = texcoord[1] - texcoord[0];
                glm::vec2 delta_uv2 = texcoord[2] - texcoord[1];

                auto divide = delta_uv1.x * delta_uv2.y - delta_uv2.x * delta_uv1.y;
                if (divide >= 0.0f && divide < 0.000001f) {
                    divide = 0.000001f;
                } else if (divide < 0.0f && divide > -0.000001f) {
                    divide = -0.000001f;
                }

                float df = 1.0f / divide;
                tangent.x = df * (delta_uv2.y * edge1.x - delta_uv1.y * edge2.x);
                tangent.y = df * (delta_uv2.y * edge1.y - delta_uv1.y * edge2.y);
                tangent.z = df * (delta_uv2.y * edge1.z - delta_uv1.y * edge2.z);
                tangent = glm::normalize(tangent);
            }

            for (size_t i = 0; i < 3; ++i) {
                MeshVertex vertex{};

                vertex.position = position[i];
                vertex.normal = normal[i];
                vertex.tangent = tangent;
                vertex.texcoord = texcoord[i];

                if (unique_vertices.count(vertex) == 0) {
                    unique_vertices[vertex] =
                        static_cast<uint16_t>(mesh_data.vertices.size());
                    mesh_data.vertices.push_back(vertex);
                    aabb.merge(vertex.position);
                }
                mesh_data.indices.push_back(unique_vertices[vertex]);
            }
        }
    }

    return mesh_data;
}

}  // namespace Vain