#include "render_system.h"

#include "function/global/global_context.h"
#include "function/render/window_system.h"
#include "resource/asset_manager.h"
#include "resource/asset_type.h"
#include "resource/config_manager.h"

namespace Vain {

RenderSystem::~RenderSystem() { clear(); }

void RenderSystem::initialize(WindowSystem *window_system) {
    ConfigManager *config_manager = g_runtime_global_context.config_manager.get();
    AssetManager *asset_manager = g_runtime_global_context.asset_manager.get();

    m_ctx = std::make_unique<VulkanContext>();
    m_ctx->initialize(window_system);

    g_runtime_global_context.window_system->registerOnWindowSizeFunc(
        [this](int w, int h) { m_ctx->recreate_swapchain = true; }
    );

    GlobalRenderDesc global_render_desc{};
    asset_manager->loadAsset(
        config_manager->getGlobalRenderDescUrl().string(), global_render_desc
    );

    IBLDesc ibl_desc{};
    ibl_desc.brdf_map = global_render_desc.brdf_map;
    ibl_desc.skybox_irradiance_map = global_render_desc.skybox_irradiance_map;
    ibl_desc.skybox_specular_map = global_render_desc.skybox_specular_map;

    m_render_resource = std::make_unique<RenderResource>();
    m_render_resource->initialize(m_ctx.get());
    m_render_resource->uploadGlobalRenderResource(ibl_desc);

    const CameraConfig &camera_config = global_render_desc.camera_config;
    m_render_camera = std::make_unique<RenderCamera>();
    m_render_camera->lookAt(
        camera_config.pose.position, camera_config.pose.target, camera_config.pose.up
    );
    m_render_camera->zfar = camera_config.z_far;
    m_render_camera->znear = camera_config.z_near;
    m_render_camera->aspect = camera_config.aspect.x / camera_config.aspect.y;

    m_render_scene = std::make_unique<RenderScene>();
    m_render_scene->ambient_light = global_render_desc.ambient_light;
    m_render_scene->directional_light = global_render_desc.directional_light;

    m_point_light_pass = std::make_unique<PointLightPass>();
    RenderPassInitInfo point_light_pass_info{m_ctx.get(), m_render_resource.get()};
    m_point_light_pass->initialize(&point_light_pass_info);

    m_directional_light_pass = std::make_unique<DirectionalLightPass>();
    RenderPassInitInfo directional_light_pass_info{m_ctx.get(), m_render_resource.get()};
    m_directional_light_pass->initialize(&directional_light_pass_info);

    m_main_pass = std::make_unique<MainPass>();
    MainPassInitInfo main_pass_info{
        m_ctx.get(),
        m_render_resource.get(),
        m_point_light_pass->framebuffer_info.attachments[0].view,
        m_directional_light_pass->framebuffer_info.attachments[0].view
    };
    m_main_pass->initialize(&main_pass_info);

    m_tone_mapping_pass = std::make_unique<ToneMappingPass>();
    ToneMappingPassInitInfo tone_mapping_pass_info{
        m_ctx.get(),
        m_render_resource.get(),
        m_main_pass->render_pass,
        m_main_pass->framebuffer_info.attachments[_backup_buffer_odd].view
    };
    m_tone_mapping_pass->initialize(&tone_mapping_pass_info);

    m_ui_pass = std::make_unique<UIPass>();
    UIPassInitInfo ui_pass_info{
        m_ctx.get(), m_render_resource.get(), m_main_pass->render_pass
    };
    m_ui_pass->initialize(&ui_pass_info);

    m_combine_ui_pass = std::make_unique<CombineUIPass>();
    CombineUIPassInitInfo combine_ui_pass_info{
        m_ctx.get(),
        m_render_resource.get(),
        m_main_pass->render_pass,
        m_main_pass->framebuffer_info.attachments[_backup_buffer_even].view,
        m_main_pass->framebuffer_info.attachments[_backup_buffer_odd].view,
    };
    m_combine_ui_pass->initialize(&combine_ui_pass_info);
}

void RenderSystem::clear() {
    vkDeviceWaitIdle(m_ctx->device);

    m_combine_ui_pass.reset();
    m_ui_pass.reset();
    m_tone_mapping_pass.reset();
    m_main_pass.reset();
    m_directional_light_pass.reset();
    m_point_light_pass.reset();

    m_render_scene.reset();

    m_render_camera.reset();

    m_render_resource.reset();

    m_ctx.reset();
}

void RenderSystem::initializeUIRenderBackend(WindowUI *window_ui) {
    m_ui_pass->initializeUIRenderBackend(window_ui);
}

void RenderSystem::tick(float delta_time) {
    processSwapData();

    m_render_resource->updatePerFrame(*m_render_scene, *m_render_camera);

    m_render_scene->updateVisibleNodes(*m_render_resource, *m_render_camera);

    render();
}

void RenderSystem::processSwapData() {
    auto swap_data = m_swap_context.getRenderSwapData();

    AssetManager *asset_manager = g_runtime_global_context.asset_manager.get();
    assert(asset_manager);

    if (swap_data->ibl_swap_data.has_value()) {
        m_render_resource->uploadGlobalRenderResource(swap_data->ibl_swap_data.value());

        swap_data->ibl_swap_data.reset();
    }

    if (swap_data->dirty_game_objects.has_value()) {
        while (!swap_data->dirty_game_objects->empty()) {
            GameObjectDesc gobject = swap_data->dirty_game_objects->front();

            for (size_t part_id = 0; part_id < gobject.object_parts.size(); ++part_id) {
                const auto &gobject_part = gobject.object_parts[part_id];
                GObjectPartID go_part_id = {gobject.go_id, part_id};

                bool is_entity_in_scene =
                    m_render_scene->entity_id_allocator.hasAsset(go_part_id);
                RenderEntity render_entity{};
                render_entity.entity_id =
                    m_render_scene->entity_id_allocator.allocateGuid(go_part_id);
                render_entity.model_matrix = gobject_part.transform.matrix();

                MeshDesc mesh_desc = {gobject_part.mesh_desc.mesh_file};
                bool is_mesh_loaded =
                    m_render_scene->mesh_guid_allocator.hasAsset(mesh_desc);
                MeshData mesh_data{};
                if (!is_mesh_loaded) {
                    mesh_data =
                        loadMeshDataFromObjFile(mesh_desc.mesh_file, render_entity.aabb);
                } else {
                    render_entity.aabb = m_render_scene->aabb_cache[mesh_desc];
                }
                render_entity.mesh_asset_id =
                    m_render_scene->mesh_guid_allocator.allocateGuid(mesh_desc);

                PBRMaterialDesc material_desc{};
                if (gobject_part.material_desc.with_texture) {
                    material_desc = {
                        material_desc.base_color_file,
                        material_desc.normal_file,
                        material_desc.metallic_file,
                        material_desc.roughness_file,
                        material_desc.occlusion_file,
                        material_desc.emissive_file
                    };
                }
                bool is_material_loaded =
                    m_render_scene->pbr_material_guid_allocator.hasAsset(material_desc);
                PBRMaterialData material_data{};
                if (!is_material_loaded) {
                    material_data = loadPBRMaterial(material_desc);
                }
                render_entity.material_asset_id =
                    m_render_scene->pbr_material_guid_allocator.allocateGuid(material_desc
                    );

                if (!is_mesh_loaded) {
                    m_render_resource->uploadMesh(render_entity, mesh_data);
                }

                if (!is_material_loaded) {
                    m_render_resource->uploadPBRMaterial(render_entity, material_data);
                }

                if (!is_entity_in_scene) {
                    m_render_scene->render_entities.push_back(render_entity);
                } else {
                    for (auto &entity : m_render_scene->render_entities) {
                        if (entity.entity_id == render_entity.entity_id) {
                            entity = render_entity;
                        }
                    }
                }
            }

            swap_data->dirty_game_objects->pop_front();
        }

        swap_data->dirty_game_objects.reset();
    }
}

void RenderSystem::passUpdateAfterRecreateSwapchain() {
    m_main_pass->onResize();
    m_tone_mapping_pass->onResize(
        m_main_pass->framebuffer_info.attachments[_backup_buffer_odd].view
    );
    m_combine_ui_pass->onResize(
        m_main_pass->framebuffer_info.attachments[_backup_buffer_even].view,
        m_main_pass->framebuffer_info.attachments[_backup_buffer_odd].view
    );
}

void RenderSystem::render() {
    m_render_resource->resetRingBufferOffset();

    m_ctx->waitForFlight();

    vkResetCommandPool(m_ctx->device, m_ctx->currentCommandPool(), 0);

    bool recreate_swapchain =
        m_ctx->prepareBeforePass([this]() { passUpdateAfterRecreateSwapchain(); });
    if (recreate_swapchain) {
        return;
    }

    m_directional_light_pass->draw(*m_render_scene);
    m_point_light_pass->draw(*m_render_scene);

    if (pipeline_type == PipelineType::DEFERRED) {
        m_main_pass->draw(
            *m_render_scene, *m_tone_mapping_pass, *m_ui_pass, *m_combine_ui_pass
        );
    } else {
        m_main_pass->drawForward(
            *m_render_scene, *m_tone_mapping_pass, *m_ui_pass, *m_combine_ui_pass
        );
    }

    m_ctx->submitRendering([this]() { passUpdateAfterRecreateSwapchain(); });
}

}  // namespace Vain