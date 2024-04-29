#include "render_object.h"

#include <assimp/material.h>
#include <assimp/postprocess.h>
#include <assimp/scene.h>

#include <assimp/Importer.hpp>

#include "function/global/global_context.h"
#include "function/render/render_data.h"
#include "function/render/render_resource.h"
#include "resource/asset_manager.h"

namespace Vain {

MeshData processMeshData(aiMesh *mesh, const aiScene *scene) {
    MeshData data{};

    data.vertices.reserve(mesh->mNumVertices);

    for (unsigned int i = 0; i < mesh->mNumVertices; ++i) {
        MeshVertex vertex{};

        vertex.position.x = mesh->mVertices[i].x;
        vertex.position.y = mesh->mVertices[i].y;
        vertex.position.z = mesh->mVertices[i].z;

        data.aabb.merge(vertex.position);

        if (mesh->HasNormals()) {
            vertex.normal.x = mesh->mNormals[i].x;
            vertex.normal.y = mesh->mNormals[i].y;
            vertex.normal.z = mesh->mNormals[i].z;
        }

        if (mesh->HasTangentsAndBitangents()) {
            vertex.tangent.x = mesh->mTangents[i].x;
            vertex.tangent.y = mesh->mTangents[i].y;
            vertex.tangent.z = mesh->mTangents[i].z;
        }

        if (mesh->mTextureCoords[0]) {
            vertex.texcoord.x = mesh->mTextureCoords[0][i].x;
            vertex.texcoord.y = mesh->mTextureCoords[0][i].y;
        } else {
            vertex.texcoord = {0.5, 0.5};
        }

        data.vertices.push_back(vertex);
    }

    for (unsigned int i = 0; i < mesh->mNumFaces; ++i) {
        aiFace face = mesh->mFaces[i];
        for (unsigned int j = 0; j < face.mNumIndices; ++j) {
            data.indices.push_back(face.mIndices[j]);
        }
    }

    return data;
}

std::shared_ptr<GameObjectNode> GameObjectNode::load(
    const aiNode *node,
    const aiScene *scene,
    const std::string &url,
    RenderScene &render_scene,
    RenderResource &render_resource,
    glm::mat4 parent_model
) {
    auto go_node = std::make_shared<GameObjectNode>();

    glm::mat4 local_model;
    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 4; ++j) {
            local_model[i][j] = node->mTransformation[j][i];
        }
    }
    go_node->original_model = parent_model * local_model;

    for (unsigned int i = 0; i < node->mNumMeshes; ++i) {
        auto entity = std::make_shared<RenderEntity>();

        aiMesh *mesh = scene->mMeshes[node->mMeshes[i]];
        MeshDesc mesh_desc = {url + "::" + mesh->mName.C_Str()};
        bool mesh_loaded = render_scene.mesh_guid_allocator.hasAsset(mesh_desc);
        entity->mesh_asset_id = render_scene.mesh_guid_allocator.allocateGuid(mesh_desc);

        if (!mesh_loaded) {
            auto mesh_data = processMeshData(mesh, scene);
            render_resource.uploadMesh(*entity, mesh_data);
            entity->aabb = mesh_data.aabb;
        } else {
            entity->aabb = render_resource.getEntityMesh(*entity)->aabb;
        }

        aiMaterial *material = scene->mMaterials[mesh->mMaterialIndex];
        PBRMaterialDesc material_desc{};
        auto dir = std::filesystem::path{url}.parent_path();
        if (material->GetTextureCount(aiTextureType_BASE_COLOR)) {
            aiString file;
            material->GetTexture(aiTextureType_BASE_COLOR, 0, &file);
            material_desc.base_color_file = (dir / file.C_Str()).generic_string();
        }
        if (material->GetTextureCount(aiTextureType_UNKNOWN)) {
            // metallic roughness
            aiString file;
            material->GetTexture(aiTextureType_UNKNOWN, 0, &file);
            material_desc.metallic_roughness_file = (dir / file.C_Str()).generic_string();
        }
        if (material->GetTextureCount(aiTextureType_NORMALS)) {
            aiString file;
            material->GetTexture(aiTextureType_NORMALS, 0, &file);
            material_desc.normal_file = (dir / file.C_Str()).generic_string();
        }
        if (material->GetTextureCount(aiTextureType_AMBIENT_OCCLUSION)) {
            aiString file;
            material->GetTexture(aiTextureType_AMBIENT_OCCLUSION, 0, &file);
            material_desc.occlusion_file = (dir / file.C_Str()).generic_string();
        }
        if (material->GetTextureCount(aiTextureType_EMISSIVE)) {
            aiString file;
            material->GetTexture(aiTextureType_EMISSIVE, 0, &file);
            material_desc.emissive_file = (dir / file.C_Str()).generic_string();
        }
        bool material_loaded =
            render_scene.material_guid_allocator.hasAsset(material_desc);
        entity->material_asset_id =
            render_scene.material_guid_allocator.allocateGuid(material_desc);
        if (!material_loaded) {
            PBRMaterialData material_data = loadPBRMaterial(material_desc);
            render_resource.uploadPBRMaterial(*entity, material_data);
        }

        render_scene.render_entities.insert(entity);
        go_node->entities.push_back(entity);
    }

    for (unsigned int i = 0; i < node->mNumChildren; ++i) {
        go_node->children.emplace_back(load(
            node->mChildren[i],
            scene,
            url,
            render_scene,
            render_resource,
            go_node->original_model
        ));
    }

    return go_node;
}

void GameObjectNode::clone(const std::shared_ptr<GameObjectNode> &node) {
    original_model = node->original_model;
    for (auto &entity : node->entities) {
        entities.emplace_back(std::make_shared<RenderEntity>(*entity));
        entities.back()->model_matrix = original_model;
    }

    for (auto &child : node->children) {
        children.emplace_back(std::make_shared<GameObjectNode>());
        children.back()->clone(child);
    }
}

void GameObjectNode::updateTransform(glm::mat4 transform) {
    for (auto &entity : entities) {
        entity->model_matrix = transform * original_model;
    }

    for (auto &child : children) {
        updateTransform(transform);
    }
}

void GameObject::load(RenderScene &render_scene, RenderResource &render_resource) {
    auto asset_manager = g_runtime_global_context.asset_manager.get();

    Assimp::Importer importer;
    auto scene = importer.ReadFile(
        asset_manager->getFullPath(url).generic_string(),
        aiProcess_Triangulate | aiProcess_GenSmoothNormals | aiProcess_FlipUVs |
            aiProcess_CalcTangentSpace
    );

    if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) {
        VAIN_ERROR("failed to load {}: {} ", url, importer.GetErrorString());
        return;
    }

    root_node = GameObjectNode::load(
        scene->mRootNode, scene, url, render_scene, render_resource, m_transform.matrix()
    );

    go_id = ObjectIDAllocator::alloc();
    m_loaded = true;
}

void GameObject::clone(const GameObject &gobject) {
    if (!gobject.loaded()) {
        return;
    }

    url = gobject.url;
    root_node = std::make_shared<GameObjectNode>();

    root_node->clone(gobject.root_node);

    go_id = ObjectIDAllocator::alloc();
    m_loaded = true;
}

void GameObject::updateTransform(Transform transform) {
    m_transform = transform;
    root_node->updateTransform(m_transform.matrix());
}

}  // namespace Vain