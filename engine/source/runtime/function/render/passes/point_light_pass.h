#pragma once

#include "function/render/render_pass.h"
#include "function/render/render_type.h"

namespace Vain {

class RenderScene;

class PointLightPass : public RenderPass {
  public:
    PointLightPass() = default;
    ~PointLightPass();

    virtual void initialize(RenderPassInitInfo *init_info) override;
    virtual void clear() override;

    void preparePassData();
    void draw(const RenderScene &scene);

  private:
    PointLightShadowPerFrameStorageBufferObject
        m_point_light_shadow_per_frame_storage_buffer_object;

    void createAttachments();
    void createRenderPass();
    void createDescriptorSetLayouts();
    void createPipelines();
    void allocateDescriptorSets();
    void createFramebuffer();
};

}  // namespace Vain