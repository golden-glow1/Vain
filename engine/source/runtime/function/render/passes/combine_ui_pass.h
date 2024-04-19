#pragma once

#include "function/render/render_pass.h"

namespace Vain {

struct CombineUIPassInitInfo : public RenderPassInitInfo {
    VkRenderPass render_pass;
    VkImageView scene_input_attachment;
    VkImageView ui_input_attachment;
};

class CombineUIPass : public RenderPass {
  public:
    CombineUIPass() = default;
    ~CombineUIPass();

    virtual void initialize(RenderPassInitInfo *init_info) override;
    virtual void clear() override;
    void draw();

    void onResize(VkImageView scene_input_attachment, VkImageView ui_input_attachment);

  private:
    void createDecriptorSetLayouts();
    void createPipelines();
    void allocateDecriptorSets();
};

}  // namespace Vain