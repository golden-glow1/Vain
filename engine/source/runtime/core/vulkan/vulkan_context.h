#pragma once

#include <vk_mem_alloc.h>
#include <vulkan/vulkan.h>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <array>
#include <cstdint>
#include <functional>
#include <map>
#include <optional>
#include <vector>

namespace Vain {

enum class DefaultSamplerType { DEFAULT_SAMPLER_LINEAR, DEFAULT_SAMPLER_NEAREST };

struct QueueFamilyIndices {
    std::optional<uint32_t> graphics_family;
    std::optional<uint32_t> present_family;
    std::optional<uint32_t> compute_family;

    QueueFamilyIndices() = default;
    QueueFamilyIndices(VkPhysicalDevice physical_device, VkSurfaceKHR surface);

    operator bool() const {
        return graphics_family.has_value() && present_family.has_value() &&
               compute_family.has_value();
    }
};

struct SwapChainSupportDetails {
    VkSurfaceCapabilitiesKHR capabilities;
    std::vector<VkSurfaceFormatKHR> formats;
    std::vector<VkPresentModeKHR> present_modes;

    SwapChainSupportDetails(VkPhysicalDevice physical_device, VkSurfaceKHR surface);

    operator bool() const { return !formats.empty() && !present_modes.empty(); }
};

class WindowSystem;

class VulkanContext {
  public:
    static constexpr uint32_t k_max_frames_in_flight{3};
    static constexpr uint32_t k_max_material_count{256};
    // static constexpr uint32_t k_max_vertex_blending_mesh_count{256};

#ifndef NDEBUG
    static constexpr bool s_enable_validation_layers = true;
    static constexpr bool s_enable_debug_utils_label = true;
#else
    static constexpr bool s_enable_validation_layers = false;
    static constexpr bool s_enable_debug_utils_label = false;
#endif

    bool recreate_swapchain{false};

    GLFWwindow *window{nullptr};

    VkInstance instance{};
    VkSurfaceKHR surface{};

    VkPhysicalDevice physical_device{};
    VkDevice device{};

    QueueFamilyIndices queue_indices{};
    VkQueue graphics_queue{};
    VkQueue present_queue{};
    VkQueue compute_queue{};

    VkCommandPool command_pool{};
    VkCommandPool command_pools_per_frame[k_max_frames_in_flight]{};
    VkCommandBuffer command_buffers_per_frame[k_max_frames_in_flight]{};

    VkDescriptorPool descriptor_pool{};

    VkSemaphore image_available_for_render_semaphores[k_max_frames_in_flight]{};
    VkSemaphore image_finished_for_presentation_semaphores[k_max_frames_in_flight]{};
    VkFence is_frame_in_flight_fences[k_max_frames_in_flight]{};

    VkSwapchainKHR swapchain{};
    VkFormat swapchain_image_format{};
    VkExtent2D swapchain_extent{};
    std::vector<VkImage> swapchain_images{};
    std::vector<VkImageView> swapchain_image_views{};

    VkFormat depth_image_format{};
    VkImage depth_image{};
    VkDeviceMemory depth_image_memory{};
    VkImageView depth_image_view{};

    VmaAllocator assets_allocator{};

    // function pointers
    PFN_vkWaitForFences waitForFences{};
    PFN_vkResetFences resetFences{};
    PFN_vkResetCommandPool resetCommandPool{};
    PFN_vkBeginCommandBuffer beginCommandBuffer{};
    PFN_vkEndCommandBuffer endCommandBuffer{};
    PFN_vkCmdBeginRenderPass cmdBeginRenderPass{};
    PFN_vkCmdNextSubpass cmdNextSubpass{};
    PFN_vkCmdEndRenderPass cmdEndRenderPass{};
    PFN_vkCmdBindPipeline cmdBindPipeline{};
    PFN_vkCmdSetViewport cmdSetViewport{};
    PFN_vkCmdSetScissor cmdSetScissor{};
    PFN_vkCmdBindVertexBuffers cmdBindVertexBuffers{};
    PFN_vkCmdBindIndexBuffer cmdBindIndexBuffer{};
    PFN_vkCmdBindDescriptorSets cmdBindDescriptorSets{};
    PFN_vkCmdDraw cmdDraw{};
    PFN_vkCmdDrawIndexed cmdDrawIndexed{};
    PFN_vkCmdClearAttachments cmdClearAttachments{};

    VulkanContext() = default;
    ~VulkanContext();

    void initialize(WindowSystem *window_system);
    void clear();

    VkCommandBuffer beginSingleTimeCommands();
    void endSingleTimeCommands(VkCommandBuffer command_buffer);

    void waitForFlight() const;

    bool prepareBeforePass(std::function<void()> passUpdateAfterRecreateSwapchain);
    void submitRendering(std::function<void()> passUpdateAfterRecreateSwapchain);

    void pushEvent(VkCommandBuffer commond_buffer, const char *name, const float *color);
    void popEvent(VkCommandBuffer command_buffer);

    VkCommandPool currentCommandPool() const {
        return command_pools_per_frame[m_current_frame_index];
    }

    VkCommandBuffer currentCommandBuffer() const {
        return command_buffers_per_frame[m_current_frame_index];
    }

    uint32_t currentFrameIndex() const { return m_current_frame_index; }

    uint32_t currentSwapchainImageIndex() const {
        return m_current_swapchain_image_index;
    }

    VkSampler getOrCreateDefaultSampler(DefaultSamplerType);

    VkSampler getOrCreateMipmapSampler(uint32_t width, uint32_t height);

    bool enablePointLightShadow() const { return m_enable_point_light_shadow; }

  private:
    static constexpr uint32_t s_vulkan_api_version{VK_API_VERSION_1_0};
    static constexpr std::array s_validation_layers = {"VK_LAYER_KHRONOS_validation"};
    static constexpr std::array s_device_extensions = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};

    bool m_enable_point_light_shadow = true;

    uint32_t m_current_frame_index{};
    uint32_t m_current_swapchain_image_index{};
    VkDebugUtilsMessengerEXT m_debug_messenger{};

    VkSampler m_nearest_sampler{};
    VkSampler m_linear_sampler{};
    std::map<uint32_t, VkSampler> m_mipmap_samplers{};

    PFN_vkCmdBeginDebugUtilsLabelEXT m_vkCmdBeginDebugUtilsLabelEXT{};
    PFN_vkCmdEndDebugUtilsLabelEXT m_vkCmdEndDebugUtilsLabelEXT{};

    static std::vector<const char *> getRequiredExtensions();
    static bool checkValidationLayerSupport();
    static bool checkDeviceExtensionSupport(VkPhysicalDevice physical_device);
    static bool isDeviceSuitable(VkPhysicalDevice physical_device, VkSurfaceKHR surface);
    static VkFormat findDepthFormat(VkPhysicalDevice physical_device);

    void createInstance();
    void setupDebugMessenger();
    void createWindowSurface();
    void pickPhysicalDevice();
    void createLogicalDevice();
    void createCommandPool();
    void createCommandBuffers();
    void createDescriptorPool();
    void createSyncPrimitives();
    void createSwapchain();
    void createSwapchainImageViews();
    void createDepthImageAndView();
    void createAssetAllocator();

    void clearSwapchain();
    void recreateSwapchain();
};

}  // namespace Vain