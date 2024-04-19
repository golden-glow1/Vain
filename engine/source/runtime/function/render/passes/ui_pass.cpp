#include "ui_pass.h"

#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_vulkan.h>

#include "core/base/macro.h"
#include "function/render/ui/window_ui.h"

namespace Vain {

UIPass::~UIPass() { clear(); }

void UIPass::initialize(RenderPassInitInfo *init_info) {
    RenderPass::initialize(init_info);

    render_pass = reinterpret_cast<UIPassInitInfo *>(init_info)->render_pass;
}

void UIPass::clear() {
    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplGlfw_Shutdown();

    ImGui::DestroyContext();
}

void UIPass::draw() {
    if (m_window_ui) {
        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        m_window_ui->preRender();

        VkCommandBuffer command_buffer = m_ctx->currentCommandBuffer();

        float color[4] = {1.0f, 1.0f, 1.0f, 1.0f};
        m_ctx->pushEvent(command_buffer, "ImGUI", color);

        ImGui::Render();

        ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), command_buffer);

        m_ctx->popEvent(command_buffer);
    }
}

void UIPass::initializeUIRenderBackend(WindowUI *window_ui) {
    m_window_ui = window_ui;

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    ImGuiIO &io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    io.ConfigDockingAlwaysTabBar = true;
    io.ConfigWindowsMoveFromTitleBarOnly = true;

    ImGui_ImplGlfw_InitForVulkan(m_ctx->window, true);
    ImGui_ImplVulkan_InitInfo init_info{};
    init_info.Instance = m_ctx->instance;
    init_info.PhysicalDevice = m_ctx->physical_device;
    init_info.Device = m_ctx->device;
    init_info.QueueFamily = m_ctx->queue_indices.graphics_family.value();
    init_info.Queue = m_ctx->graphics_queue;
    init_info.DescriptorPool = m_ctx->descriptor_pool;
    init_info.RenderPass = render_pass;

    init_info.MinImageCount = 3;
    init_info.ImageCount = 3;
    init_info.Subpass = _subpass_ui;
    ImGui_ImplVulkan_Init(&init_info);
}

}  // namespace Vain