#pragma once

#include <memory>

#include "core/vulkan/vulkan_context.h"
#include "function/render/passes/combine_ui_pass.h"
#include "function/render/passes/directional_light_pass.h"
#include "function/render/passes/main_pass.h"
#include "function/render/passes/point_light_pass.h"
#include "function/render/passes/tone_mapping_pass.h"
#include "function/render/passes/ui_pass.h"
#include "function/render/render_camera.h"
#include "function/render/render_object.h"
#include "function/render/render_resource.h"
#include "function/render/render_scene.h"

namespace Vain {

class WindowSystem;
class UIPass;

class WindowUI;

enum class PipelineType { FORWARD, DEFERRED };

class RenderSystem {
  public:
    PipelineType pipeline_type = PipelineType::DEFERRED;

    RenderSystem() = default;
    ~RenderSystem();

    void initialize(WindowSystem *window_system);
    void clear();

    void initializeUIRenderBackend(WindowUI *window_ui);

    void tick(float delta_time);

    RenderCamera *getRenderCamera() { return m_render_camera.get(); }

    void spawnObject(const std::string &url);

    std::vector<GameObject *> getObjects();

  private:
    std::unique_ptr<VulkanContext> m_ctx{};
    std::unique_ptr<RenderResource> m_render_resource{};
    std::unique_ptr<RenderCamera> m_render_camera{};
    std::unique_ptr<RenderScene> m_render_scene{};

    std::unique_ptr<PointLightPass> m_point_light_pass{};
    std::unique_ptr<DirectionalLightPass> m_directional_light_pass{};
    std::unique_ptr<MainPass> m_main_pass{};
    std::unique_ptr<ToneMappingPass> m_tone_mapping_pass{};
    std::unique_ptr<UIPass> m_ui_pass{};
    std::unique_ptr<CombineUIPass> m_combine_ui_pass{};

    std::unordered_map<std::string, std::unordered_set<GObjectID>> m_url_go{};
    std::unordered_map<GObjectID, GameObject> m_gobjects{};

    void passUpdateAfterRecreateSwapchain();

    void render();
};

}  // namespace Vain