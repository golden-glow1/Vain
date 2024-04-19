#include "render_pass.h"

namespace Vain {

RenderPass::~RenderPass() { clear(); }

void RenderPass::initialize(RenderPassInitInfo *init_info) {
    m_ctx = init_info->ctx;
    m_res = init_info->res;
}

void RenderPass::clear() {}

}  // namespace Vain