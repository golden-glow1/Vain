#include "render_system.h"

#include <algorithm>

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

    SceneGlobalDesc scene_global_desc{};
    asset_manager->loadAsset(
        config_manager->getSceneGlobalDescUrl().generic_string(), scene_global_desc
    );

    IBLDesc ibl_desc{};
    ibl_desc.brdf_map = scene_global_desc.brdf_map;
    ibl_desc.skybox_irradiance_map = scene_global_desc.skybox_irradiance_map;
    ibl_desc.skybox_specular_map = scene_global_desc.skybox_specular_map;

    m_render_resource = std::make_unique<RenderResource>();
    m_render_resource->initialize(m_ctx.get());
    m_render_resource->uploadGlobalRenderResource(ibl_desc);

    const CameraConfig &camera_config = scene_global_desc.camera_config;
    m_render_camera = std::make_unique<RenderCamera>();
    m_render_camera->lookAt(
        camera_config.pose.position, camera_config.pose.target, camera_config.pose.up
    );
    m_render_camera->zfar = camera_config.z_far;
    m_render_camera->znear = camera_config.z_near;
    m_render_camera->aspect = camera_config.aspect.x / camera_config.aspect.y;

    m_render_scene = std::make_unique<RenderScene>();
    m_render_scene->ambient_light = scene_global_desc.ambient_light;
    m_render_scene->directional_light = scene_global_desc.directional_light;

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

    m_render_resource->material_descriptor_set_layout =
        m_main_pass->descriptor_set_layouts[MainPass::_layout_type_mesh_per_material];
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

void RenderSystem::spawnObject(const std::string &url) {}

void RenderSystem::processSwapData() {}

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