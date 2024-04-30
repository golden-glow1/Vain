#include "vulkan_context.h"

#include <cstdio>
#include <iostream>
#include <limits>
#include <set>

#include "core/base/macro.h"
#include "function/render/window_system.h"
#include "vulkan_utils.h"

#define VMA_IMPLEMENTATION 1
#define VMA_STATIC_VULKAN_FUNCTIONS 0
#define VMA_DYNAMIC_VULKAN_FUNCTIONS 1
#include <vk_mem_alloc.h>

namespace Vain {

static VkResult _vkCreateDebugUtilsMessengerEXT(
    VkInstance instance,
    const VkDebugUtilsMessengerCreateInfoEXT *pCreateInfo,
    const VkAllocationCallbacks *pAllocator,
    VkDebugUtilsMessengerEXT *pDebugMessenger
) {
    auto func = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(
        vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT")
    );
    if (func != nullptr) {
        return func(instance, pCreateInfo, pAllocator, pDebugMessenger);
    } else {
        return VK_ERROR_EXTENSION_NOT_PRESENT;
    }
}

static void _vkDestroyDebugUtilsMessengerEXT(
    VkInstance instance,
    VkDebugUtilsMessengerEXT debugMessenger,
    const VkAllocationCallbacks *pAllocator
) {
    auto func = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(
        vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT")
    );
    if (func != nullptr) {
        func(instance, debugMessenger, pAllocator);
    }
}

static VKAPI_ATTR VkBool32 VKAPI_CALL
debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT severity, VkDebugUtilsMessageTypeFlagsEXT, const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData, void *) {
    std::cout << pCallbackData->pMessage << std::endl;
    return VK_FALSE;
}

static void populateDebugMessengerCreateInfo(
    VkDebugUtilsMessengerCreateInfoEXT &create_info
) {
    create_info = {};
    create_info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    create_info.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                                  VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    create_info.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                              VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    create_info.pfnUserCallback = debugCallback;
}

QueueFamilyIndices::QueueFamilyIndices(
    VkPhysicalDevice physical_device, VkSurfaceKHR surface
) {
    uint32_t queue_family_count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(
        physical_device, &queue_family_count, nullptr
    );
    std::vector<VkQueueFamilyProperties> queue_families(queue_family_count);
    vkGetPhysicalDeviceQueueFamilyProperties(
        physical_device, &queue_family_count, queue_families.data()
    );

    uint32_t i = 0;
    for (const auto &queue_family : queue_families) {
        if (queue_family.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            graphics_family = i;
        }

        if (queue_family.queueFlags & VK_QUEUE_COMPUTE_BIT) {
            compute_family = i;
        }

        VkBool32 is_present_support = false;
        vkGetPhysicalDeviceSurfaceSupportKHR(
            physical_device, i, surface, &is_present_support
        );
        if (is_present_support) {
            present_family = i;
        }

        if (*this) {
            break;
        }
        i++;
    }
}

SwapChainSupportDetails::SwapChainSupportDetails(
    VkPhysicalDevice physical_device, VkSurfaceKHR surface
) {
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physical_device, surface, &capabilities);

    uint32_t format_count;
    vkGetPhysicalDeviceSurfaceFormatsKHR(
        physical_device, surface, &format_count, nullptr
    );
    if (format_count != 0) {
        formats.resize(format_count);
        vkGetPhysicalDeviceSurfaceFormatsKHR(
            physical_device, surface, &format_count, formats.data()
        );
    }

    uint32_t present_mode_count;
    vkGetPhysicalDeviceSurfacePresentModesKHR(
        physical_device, surface, &present_mode_count, nullptr
    );

    if (present_mode_count != 0) {
        present_modes.resize(present_mode_count);
        vkGetPhysicalDeviceSurfacePresentModesKHR(
            physical_device, surface, &present_mode_count, present_modes.data()
        );
    }
}

VulkanContext::~VulkanContext() { clear(); }

void VulkanContext::initialize(WindowSystem *window_system) {
    window = window_system->getWindow();

    auto window_size = window_system->getWindowSize();

    createInstance();

    setupDebugMessenger();

    createWindowSurface();

    pickPhysicalDevice();

    createLogicalDevice();

    createCommandPool();

    createCommandBuffers();

    createDescriptorPool();

    createSyncPrimitives();

    createSwapchain();

    createSwapchainImageViews();

    createDepthImageAndView();

    createAssetAllocator();
}

void VulkanContext::clear() {
    for (auto [level, sampler] : m_mipmap_samplers) {
        vkDestroySampler(device, sampler, nullptr);
    }
    m_mipmap_samplers.clear();

    if (m_linear_sampler) {
        vkDestroySampler(device, m_linear_sampler, nullptr);
        m_linear_sampler = VK_NULL_HANDLE;
    }
    if (m_nearest_sampler) {
        vkDestroySampler(device, m_nearest_sampler, nullptr);
        m_nearest_sampler = VK_NULL_HANDLE;
    }

    vmaDestroyAllocator(assets_allocator);

    vkDestroyImageView(device, depth_image_view, nullptr);
    vkDestroyImage(device, depth_image, nullptr);
    vkFreeMemory(device, depth_image_memory, nullptr);

    clearSwapchain();

    for (uint32_t i = 0; i < k_max_frames_in_flight; ++i) {
        vkDestroyFence(device, is_frame_in_flight_fences[i], nullptr);
        vkDestroySemaphore(device, image_available_for_render_semaphores[i], nullptr);
        vkDestroySemaphore(
            device, image_finished_for_presentation_semaphores[i], nullptr
        );
    }

    vkDestroyDescriptorPool(device, descriptor_pool, nullptr);

    for (auto cmd_pool : command_pools_per_frame) {
        vkDestroyCommandPool(device, cmd_pool, nullptr);
    }
    vkDestroyCommandPool(device, command_pool, nullptr);
    vkDestroyDevice(device, nullptr);
    vkDestroySurfaceKHR(instance, surface, nullptr);

    if (s_enable_validation_layers) {
        _vkDestroyDebugUtilsMessengerEXT(instance, m_debug_messenger, nullptr);
    }

    vkDestroyInstance(instance, nullptr);
}

VkCommandBuffer VulkanContext::beginSingleTimeCommands() {
    VkCommandBufferAllocateInfo alloc_info{};
    alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    alloc_info.commandPool = command_pool;
    alloc_info.commandBufferCount = 1;

    VkCommandBuffer command_buffer;
    vkAllocateCommandBuffers(device, &alloc_info, &command_buffer);

    VkCommandBufferBeginInfo begin_info{};
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    beginCommandBuffer(command_buffer, &begin_info);
    return command_buffer;
}

void VulkanContext::endSingleTimeCommands(VkCommandBuffer command_buffer) {
    endCommandBuffer(command_buffer);

    VkSubmitInfo submit_info{};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &command_buffer;

    vkQueueSubmit(graphics_queue, 1, &submit_info, VK_NULL_HANDLE);
    vkQueueWaitIdle(graphics_queue);

    vkFreeCommandBuffers(device, command_pool, 1, &command_buffer);
}

void VulkanContext::waitForFlight() const {
    VkResult res_wait_for_fences = waitForFences(
        device, 1, &is_frame_in_flight_fences[m_current_frame_index], VK_TRUE, UINT64_MAX
    );
    if (res_wait_for_fences != VK_SUCCESS) {
        VAIN_ERROR("failed to synchronize");
    }
}

bool VulkanContext::prepareBeforePass(
    std::function<void()> passUpdateAfterRecreateSwapchain
) {
    VkResult acquire_image_result = vkAcquireNextImageKHR(
        device,
        swapchain,
        UINT64_MAX,
        image_available_for_render_semaphores[m_current_frame_index],
        VK_NULL_HANDLE,
        &m_current_swapchain_image_index
    );

    if (acquire_image_result == VK_ERROR_OUT_OF_DATE_KHR) {
        recreateSwapchain();
        passUpdateAfterRecreateSwapchain();
        return true;
    } else if (acquire_image_result == VK_SUBOPTIMAL_KHR) {
        // also acquired an image
        recreateSwapchain();
        passUpdateAfterRecreateSwapchain();

        // NULL submit to wait semaphore
        VkPipelineStageFlags wait_stages[] = {VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT};
        VkSubmitInfo submit_info{};
        submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submit_info.waitSemaphoreCount = 1;
        submit_info.pWaitSemaphores =
            &image_available_for_render_semaphores[m_current_frame_index];
        submit_info.pWaitDstStageMask = wait_stages;
        submit_info.commandBufferCount = 0;
        submit_info.pCommandBuffers = nullptr;
        submit_info.signalSemaphoreCount = 0;
        submit_info.pSignalSemaphores = nullptr;

        VkResult res_reset_fences =
            resetFences(device, 1, &is_frame_in_flight_fences[m_current_frame_index]);
        if (res_reset_fences != VK_SUCCESS) {
            VAIN_ERROR("failed to reset fences");
            return false;
        }

        VkResult res_queue_submit = vkQueueSubmit(
            graphics_queue,
            1,
            &submit_info,
            is_frame_in_flight_fences[m_current_frame_index]
        );
        if (res_queue_submit != VK_SUCCESS) {
            VAIN_ERROR("failed to submit");
            return false;
        }

        m_current_frame_index = (m_current_frame_index + 1) % k_max_frames_in_flight;
        return true;
    } else {
        if (acquire_image_result != VK_SUCCESS) {
            VAIN_ERROR("failed to acquire next image");
            return false;
        }
    }

    VkCommandBufferBeginInfo command_buffer_begin_info{};
    command_buffer_begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    command_buffer_begin_info.flags = 0;
    command_buffer_begin_info.pInheritanceInfo = nullptr;

    VkResult res_begin_command_buffer = beginCommandBuffer(
        command_buffers_per_frame[m_current_frame_index], &command_buffer_begin_info
    );

    if (res_begin_command_buffer != VK_SUCCESS) {
        VAIN_ERROR("failed to begin command buffer");
        return false;
    }

    return false;
}

void VulkanContext::submitRendering(std::function<void()> passUpdateAfterRecreateSwapchain
) {
    VkResult res_end_command_buffer =
        endCommandBuffer(command_buffers_per_frame[m_current_frame_index]);
    if (res_end_command_buffer != VK_SUCCESS) {
        VAIN_ERROR("failed to end command buffer");
        return;
    }

    VkPipelineStageFlags wait_stages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
    VkSubmitInfo submit_info = {};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.waitSemaphoreCount = 1;
    submit_info.pWaitSemaphores =
        &image_available_for_render_semaphores[m_current_frame_index];
    submit_info.pWaitDstStageMask = wait_stages;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &command_buffers_per_frame[m_current_frame_index];
    submit_info.signalSemaphoreCount = 1;
    submit_info.pSignalSemaphores =
        &image_finished_for_presentation_semaphores[m_current_frame_index];

    VkResult res_reset_fences =
        resetFences(device, 1, &is_frame_in_flight_fences[m_current_frame_index]);
    if (res_reset_fences != VK_SUCCESS) {
        VAIN_ERROR("failed to reset fences");
        return;
    }

    VkResult res_queue_submit = vkQueueSubmit(
        graphics_queue, 1, &submit_info, is_frame_in_flight_fences[m_current_frame_index]
    );
    if (res_queue_submit != VK_SUCCESS) {
        VAIN_ERROR("failed to submit");
    }

    VkPresentInfoKHR present_info = {};
    present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    present_info.waitSemaphoreCount = 1;
    present_info.pWaitSemaphores =
        &image_finished_for_presentation_semaphores[m_current_frame_index];
    present_info.swapchainCount = 1;
    present_info.pSwapchains = &swapchain;
    present_info.pImageIndices = &m_current_swapchain_image_index;

    VkResult present_result = vkQueuePresentKHR(present_queue, &present_info);
    if (present_result == VK_ERROR_OUT_OF_DATE_KHR ||
        present_result == VK_SUBOPTIMAL_KHR || recreate_swapchain) {
        recreate_swapchain = false;
        recreateSwapchain();
        passUpdateAfterRecreateSwapchain();
    } else if (present_result != VK_SUCCESS) {
        VAIN_ERROR("vkQueuePresentKHR failed!");
        return;
    }

    m_current_frame_index = (m_current_frame_index + 1) % k_max_frames_in_flight;
}

void VulkanContext::pushEvent(
    VkCommandBuffer commond_buffer, const char *name, const float *color
) {
    if (s_enable_debug_utils_label) {
        VkDebugUtilsLabelEXT label_info;
        label_info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT;
        label_info.pNext = nullptr;
        label_info.pLabelName = name;
        for (int i = 0; i < 4; ++i) {
            label_info.color[i] = color[i];
        }
        m_vkCmdBeginDebugUtilsLabelEXT(commond_buffer, &label_info);
    }
}

void VulkanContext::popEvent(VkCommandBuffer command_buffer) {
    if (s_enable_debug_utils_label) {
        m_vkCmdEndDebugUtilsLabelEXT(command_buffer);
    }
}

VkSampler VulkanContext::getOrCreateDefaultSampler(DefaultSamplerType type) {
    switch (type) {
    case DefaultSamplerType::DEFAULT_SAMPLER_LINEAR:
        if (!m_linear_sampler) {
            VkPhysicalDeviceProperties physical_device_properties{};
            vkGetPhysicalDeviceProperties(physical_device, &physical_device_properties);

            VkSamplerCreateInfo samplerInfo{};

            samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
            samplerInfo.magFilter = VK_FILTER_LINEAR;
            samplerInfo.minFilter = VK_FILTER_LINEAR;
            samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
            samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
            samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
            samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
            samplerInfo.mipLodBias = 0.0f;
            samplerInfo.anisotropyEnable = VK_FALSE;
            samplerInfo.maxAnisotropy =
                physical_device_properties.limits.maxSamplerAnisotropy;
            samplerInfo.compareEnable = VK_FALSE;
            samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
            samplerInfo.minLod = 0.0f;
            samplerInfo.maxLod = 8.0f;
            samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
            samplerInfo.unnormalizedCoordinates = VK_FALSE;

            if (vkCreateSampler(device, &samplerInfo, nullptr, &m_linear_sampler) !=
                VK_SUCCESS) {
                VAIN_ERROR("vk create sampler");
            }
        }
        return m_linear_sampler;
        break;
    case DefaultSamplerType::DEFAULT_SAMPLER_NEAREST:
        if (!m_nearest_sampler) {
            VkPhysicalDeviceProperties physical_device_properties{};
            vkGetPhysicalDeviceProperties(physical_device, &physical_device_properties);

            VkSamplerCreateInfo samplerInfo{};

            samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
            samplerInfo.magFilter = VK_FILTER_NEAREST;
            samplerInfo.minFilter = VK_FILTER_NEAREST;
            samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
            samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
            samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
            samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
            samplerInfo.mipLodBias = 0.0f;
            samplerInfo.anisotropyEnable = VK_FALSE;
            samplerInfo.maxAnisotropy =
                physical_device_properties.limits.maxSamplerAnisotropy;  // close :1.0f
            samplerInfo.compareEnable = VK_FALSE;
            samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
            samplerInfo.minLod = 0.0f;
            samplerInfo.maxLod = 8.0f;  // todo: m_irradiance_texture_miplevels
            samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
            samplerInfo.unnormalizedCoordinates = VK_FALSE;

            if (vkCreateSampler(device, &samplerInfo, nullptr, &m_nearest_sampler) !=
                VK_SUCCESS) {
                VAIN_ERROR("vk create sampler");
            }
        }
        return m_nearest_sampler;
        break;
    }
    return nullptr;
}

VkSampler VulkanContext::getOrCreateMipmapSampler(uint32_t width, uint32_t height) {
    if (width == 0 || height == 0) {
        VAIN_ERROR("invalid width or height");
    }

    VkSampler sampler{};
    uint32_t mip_levels = floor(log2(std::max(width, height))) + 1;
    auto find_sampler = m_mipmap_samplers.find(mip_levels);
    if (find_sampler != m_mipmap_samplers.end()) {
        return find_sampler->second;
    }

    VkPhysicalDeviceProperties physical_device_properties{};
    vkGetPhysicalDeviceProperties(physical_device, &physical_device_properties);

    VkSamplerCreateInfo sampler_info{};
    sampler_info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    sampler_info.magFilter = VK_FILTER_LINEAR;
    sampler_info.minFilter = VK_FILTER_LINEAR;
    sampler_info.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    sampler_info.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    sampler_info.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    sampler_info.anisotropyEnable = VK_TRUE;
    sampler_info.maxAnisotropy = physical_device_properties.limits.maxSamplerAnisotropy;
    sampler_info.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    sampler_info.unnormalizedCoordinates = VK_FALSE;
    sampler_info.compareEnable = VK_FALSE;
    sampler_info.compareOp = VK_COMPARE_OP_ALWAYS;
    sampler_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    sampler_info.maxLod = mip_levels - 1;

    if (vkCreateSampler(device, &sampler_info, nullptr, &sampler) != VK_SUCCESS) {
        VAIN_ERROR("vkCreateSampler failed!");
    }

    m_mipmap_samplers[mip_levels] = sampler;

    return sampler;
}

std::vector<const char *> VulkanContext::getRequiredExtensions() {
    uint32_t glfwExtensionCount = 0;
    const char **glfwExtensions;
    glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

    std::vector<const char *> extensions(
        glfwExtensions, glfwExtensions + glfwExtensionCount
    );

    if (s_enable_validation_layers || s_enable_debug_utils_label) {
        extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    }

    return extensions;
}

VkFormat findSupportedFormat(
    VkPhysicalDevice physical_device,
    const std::vector<VkFormat> &candidates,
    VkImageTiling tiling,
    VkFormatFeatureFlags features
) {
    for (VkFormat format : candidates) {
        VkFormatProperties props;
        vkGetPhysicalDeviceFormatProperties(physical_device, format, &props);

        if (tiling == VK_IMAGE_TILING_LINEAR &&
            (props.linearTilingFeatures & features) == features) {
            return format;
        } else if (tiling == VK_IMAGE_TILING_OPTIMAL && (props.optimalTilingFeatures & features) == features) {
            return format;
        }
    }

    VAIN_ERROR("failed to find supported format");

    return VK_FORMAT_UNDEFINED;
}

bool VulkanContext::checkValidationLayerSupport() {
    uint32_t layer_count;
    vkEnumerateInstanceLayerProperties(&layer_count, nullptr);
    std::vector<VkLayerProperties> available_layers(layer_count);
    vkEnumerateInstanceLayerProperties(&layer_count, available_layers.data());

    std::set<std::string> required_layers(
        s_validation_layers.begin(), s_validation_layers.end()
    );

    for (const auto &layer : available_layers) {
        required_layers.erase(layer.layerName);
    }

    return required_layers.empty();
}

bool VulkanContext::checkDeviceExtensionSupport(VkPhysicalDevice physical_device) {
    uint32_t extension_count;
    vkEnumerateDeviceExtensionProperties(
        physical_device, nullptr, &extension_count, nullptr
    );

    std::vector<VkExtensionProperties> available_extensions(extension_count);
    vkEnumerateDeviceExtensionProperties(
        physical_device, nullptr, &extension_count, available_extensions.data()
    );

    std::set<std::string> required_extensions(
        s_device_extensions.begin(), s_device_extensions.end()
    );
    for (const auto &extension : available_extensions) {
        required_extensions.erase(extension.extensionName);
    }

    return required_extensions.empty();
}

bool VulkanContext::isDeviceSuitable(
    VkPhysicalDevice physical_device, VkSurfaceKHR surface
) {
    QueueFamilyIndices queue_indices(physical_device, surface);
    if (!queue_indices) {
        return false;
    }

    if (!checkDeviceExtensionSupport(physical_device)) {
        return false;
    }

    SwapChainSupportDetails swapchain_support_details(physical_device, surface);
    if (!swapchain_support_details) {
        return false;
    }

    VkPhysicalDeviceFeatures physical_device_features;
    vkGetPhysicalDeviceFeatures(physical_device, &physical_device_features);

    return physical_device_features.samplerAnisotropy;
}

VkFormat VulkanContext::findDepthFormat(VkPhysicalDevice physical_device) {
    return findSupportedFormat(
        physical_device,
        {VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT},
        VK_IMAGE_TILING_OPTIMAL,
        VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT
    );
}

void VulkanContext::createInstance() {
    // validation layer will be enabled in debug mode
    if (s_enable_validation_layers && !checkValidationLayerSupport()) {
        VAIN_ERROR("validation layers requested, but not available!");
    }

    VkApplicationInfo app_info{};
    app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app_info.pApplicationName = "vain_renderer";
    app_info.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    app_info.pEngineName = "Vain";
    app_info.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    app_info.apiVersion = s_vulkan_api_version;

    VkInstanceCreateInfo instance_create_info{};
    instance_create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    instance_create_info.pApplicationInfo = &app_info;

    auto extensions = getRequiredExtensions();
    instance_create_info.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
    instance_create_info.ppEnabledExtensionNames = extensions.data();

    VkDebugUtilsMessengerCreateInfoEXT debug_create_info{};
    if (s_enable_validation_layers) {
        instance_create_info.enabledLayerCount =
            static_cast<uint32_t>(s_validation_layers.size());
        instance_create_info.ppEnabledLayerNames = s_validation_layers.data();

        populateDebugMessengerCreateInfo(debug_create_info);
        instance_create_info.pNext = &debug_create_info;
    } else {
        instance_create_info.enabledLayerCount = 0;
        instance_create_info.pNext = nullptr;
    }

    if (vkCreateInstance(&instance_create_info, nullptr, &instance) != VK_SUCCESS) {
        VAIN_ERROR("failed to create instance");
    }
}

void VulkanContext::setupDebugMessenger() {
    if (s_enable_validation_layers) {
        VkDebugUtilsMessengerCreateInfoEXT create_info;
        populateDebugMessengerCreateInfo(create_info);
        if (_vkCreateDebugUtilsMessengerEXT(
                instance, &create_info, nullptr, &m_debug_messenger
            ) != VK_SUCCESS) {
            VAIN_ERROR("failed to set up debug messenger!");
        }
    }

    if (s_enable_debug_utils_label) {
        m_vkCmdBeginDebugUtilsLabelEXT =
            reinterpret_cast<PFN_vkCmdBeginDebugUtilsLabelEXT>(
                vkGetInstanceProcAddr(instance, "vkCmdBeginDebugUtilsLabelEXT")
            );
        m_vkCmdEndDebugUtilsLabelEXT = reinterpret_cast<PFN_vkCmdEndDebugUtilsLabelEXT>(
            vkGetInstanceProcAddr(instance, "vkCmdEndDebugUtilsLabelEXT")
        );
    }
}

void VulkanContext::createWindowSurface() {
    if (glfwCreateWindowSurface(instance, window, nullptr, &surface) != VK_SUCCESS) {
        VAIN_ERROR("failed to create window surface!");
    }
}

void VulkanContext::pickPhysicalDevice() {
    uint32_t physical_device_count;
    vkEnumeratePhysicalDevices(instance, &physical_device_count, nullptr);
    if (physical_device_count == 0) {
        VAIN_ERROR("failed to enumerate physical devices!");
        return;
    }

    std::vector<VkPhysicalDevice> physical_devices(physical_device_count);
    vkEnumeratePhysicalDevices(instance, &physical_device_count, physical_devices.data());

    std::vector<std::pair<int, VkPhysicalDevice>> ranked_physical_devices;
    for (const auto &device : physical_devices) {
        VkPhysicalDeviceProperties physical_device_properties;
        vkGetPhysicalDeviceProperties(device, &physical_device_properties);
        int score = 0;

        if (physical_device_properties.deviceType ==
            VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
            score += 1000;
        } else if (physical_device_properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU) {
            score += 100;
        }

        ranked_physical_devices.push_back({score, device});
    }

    std::sort(
        ranked_physical_devices.begin(),
        ranked_physical_devices.end(),
        [](const std::pair<int, VkPhysicalDevice> &p1,
           const std::pair<int, VkPhysicalDevice> &p2) { return p1 > p2; }
    );

    for (const auto &device : ranked_physical_devices) {
        if (isDeviceSuitable(device.second, surface)) {
            physical_device = device.second;
            break;
        }
    }

    if (physical_device == VK_NULL_HANDLE) {
        VAIN_ERROR("failed to find suitable physical device");
    }
}

void VulkanContext::createLogicalDevice() {
    queue_indices = QueueFamilyIndices(physical_device, surface);

    std::vector<VkDeviceQueueCreateInfo> queue_create_infos;
    std::set<uint32_t> queue_families = {
        queue_indices.graphics_family.value(),
        queue_indices.present_family.value(),
        queue_indices.compute_family.value()
    };

    float queue_priority = 1.0f;
    for (uint32_t queue_family : queue_families) {
        VkDeviceQueueCreateInfo queue_create_info{};
        queue_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queue_create_info.queueFamilyIndex = queue_family;
        queue_create_info.queueCount = 1;
        queue_create_info.pQueuePriorities = &queue_priority;
        queue_create_infos.push_back(queue_create_info);
    }

    VkPhysicalDeviceFeatures physical_device_features = {};
    physical_device_features.samplerAnisotropy = VK_TRUE;
    physical_device_features.fragmentStoresAndAtomics = VK_TRUE;

    physical_device_features.independentBlend = VK_TRUE;

    if (m_enable_point_light_shadow) {
        physical_device_features.geometryShader = VK_TRUE;
    }

    VkDeviceCreateInfo device_create_info{};
    device_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    device_create_info.pQueueCreateInfos = queue_create_infos.data();
    device_create_info.queueCreateInfoCount =
        static_cast<uint32_t>(queue_create_infos.size());
    device_create_info.pEnabledFeatures = &physical_device_features;
    device_create_info.enabledExtensionCount =
        static_cast<uint32_t>(s_device_extensions.size());
    device_create_info.ppEnabledExtensionNames = s_device_extensions.data();
    device_create_info.enabledLayerCount = 0;

    if (vkCreateDevice(physical_device, &device_create_info, nullptr, &device) !=
        VK_SUCCESS) {
        VAIN_ERROR("failed to create device");
    }

    vkGetDeviceQueue(device, queue_indices.graphics_family.value(), 0, &graphics_queue);
    vkGetDeviceQueue(device, queue_indices.present_family.value(), 0, &present_queue);
    vkGetDeviceQueue(device, queue_indices.compute_family.value(), 0, &compute_queue);

    // more efficient pointer
    waitForFences = reinterpret_cast<PFN_vkWaitForFences>(
        vkGetDeviceProcAddr(device, "vkWaitForFences")
    );
    resetFences =
        reinterpret_cast<PFN_vkResetFences>(vkGetDeviceProcAddr(device, "vkResetFences"));
    resetCommandPool = reinterpret_cast<PFN_vkResetCommandPool>(
        vkGetDeviceProcAddr(device, "vkResetCommandPool")
    );
    beginCommandBuffer = reinterpret_cast<PFN_vkBeginCommandBuffer>(
        vkGetDeviceProcAddr(device, "vkBeginCommandBuffer")
    );
    endCommandBuffer = reinterpret_cast<PFN_vkEndCommandBuffer>(
        vkGetDeviceProcAddr(device, "vkEndCommandBuffer")
    );
    cmdBeginRenderPass = reinterpret_cast<PFN_vkCmdBeginRenderPass>(
        vkGetDeviceProcAddr(device, "vkCmdBeginRenderPass")
    );
    cmdNextSubpass = reinterpret_cast<PFN_vkCmdNextSubpass>(
        vkGetDeviceProcAddr(device, "vkCmdNextSubpass")
    );
    cmdEndRenderPass = reinterpret_cast<PFN_vkCmdEndRenderPass>(
        vkGetDeviceProcAddr(device, "vkCmdEndRenderPass")
    );
    cmdBindPipeline = reinterpret_cast<PFN_vkCmdBindPipeline>(
        vkGetDeviceProcAddr(device, "vkCmdBindPipeline")
    );
    cmdSetViewport = reinterpret_cast<PFN_vkCmdSetViewport>(
        vkGetDeviceProcAddr(device, "vkCmdSetViewport")
    );
    cmdSetScissor = reinterpret_cast<PFN_vkCmdSetScissor>(
        vkGetDeviceProcAddr(device, "vkCmdSetScissor")
    );
    cmdBindVertexBuffers = reinterpret_cast<PFN_vkCmdBindVertexBuffers>(
        vkGetDeviceProcAddr(device, "vkCmdBindVertexBuffers")
    );
    cmdBindIndexBuffer = reinterpret_cast<PFN_vkCmdBindIndexBuffer>(
        vkGetDeviceProcAddr(device, "vkCmdBindIndexBuffer")
    );
    cmdBindDescriptorSets = reinterpret_cast<PFN_vkCmdBindDescriptorSets>(
        vkGetDeviceProcAddr(device, "vkCmdBindDescriptorSets")
    );
    cmdDraw = reinterpret_cast<PFN_vkCmdDraw>(vkGetDeviceProcAddr(device, "vkCmdDraw"));
    cmdDrawIndexed = reinterpret_cast<PFN_vkCmdDrawIndexed>(
        vkGetDeviceProcAddr(device, "vkCmdDrawIndexed")
    );
    cmdClearAttachments = reinterpret_cast<PFN_vkCmdClearAttachments>(
        vkGetDeviceProcAddr(device, "vkCmdClearAttachments")
    );
}

void VulkanContext::createCommandPool() {
    {
        VkCommandPoolCreateInfo command_pool_create_info{};
        command_pool_create_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        command_pool_create_info.pNext = nullptr;
        command_pool_create_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        command_pool_create_info.queueFamilyIndex = queue_indices.graphics_family.value();

        if (vkCreateCommandPool(
                device, &command_pool_create_info, nullptr, &command_pool
            ) != VK_SUCCESS) {
            VAIN_ERROR("failed to create command pool");
        }
    }

    {
        VkCommandPoolCreateInfo command_pool_create_info;
        command_pool_create_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        command_pool_create_info.pNext = nullptr;
        command_pool_create_info.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
        command_pool_create_info.queueFamilyIndex = queue_indices.graphics_family.value();

        for (uint32_t i = 0; i < k_max_frames_in_flight; ++i) {
            if (vkCreateCommandPool(
                    device,
                    &command_pool_create_info,
                    nullptr,
                    &command_pools_per_frame[i]
                ) != VK_SUCCESS) {
                VAIN_ERROR("failed to create command pool");
            }
        }
    }
}

void VulkanContext::createCommandBuffers() {
    VkCommandBufferAllocateInfo command_buffer_allocate_info{};
    command_buffer_allocate_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    command_buffer_allocate_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    command_buffer_allocate_info.commandBufferCount = 1;

    for (uint32_t i = 0; i < k_max_frames_in_flight; ++i) {
        command_buffer_allocate_info.commandPool = command_pools_per_frame[i];
        VkCommandBuffer command_buffer;
        if (vkAllocateCommandBuffers(
                device, &command_buffer_allocate_info, &command_buffers_per_frame[i]
            ) != VK_SUCCESS) {
            VAIN_ERROR("failed to allocate command buffers");
        }
    }
}

void VulkanContext::createDescriptorPool() {
    VkDescriptorPoolSize pool_sizes[7];
    pool_sizes[0].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC;
    pool_sizes[0].descriptorCount = 3 + 2 + 2 + 2 + 1 + 1 + 3 + 3;
    pool_sizes[1].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    pool_sizes[1].descriptorCount = 2;
    pool_sizes[2].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    pool_sizes[2].descriptorCount = k_max_material_count;
    pool_sizes[3].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    pool_sizes[3].descriptorCount = 5 + 5 * k_max_material_count;
    pool_sizes[4].type = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
    pool_sizes[4].descriptorCount = 4 + 1 + 1 + 2;
    pool_sizes[5].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
    pool_sizes[5].descriptorCount = 3;
    pool_sizes[6].type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    pool_sizes[6].descriptorCount = 1;

    VkDescriptorPoolCreateInfo pool_info{};
    pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pool_info.poolSizeCount = ARRAY_SIZE(pool_sizes);
    pool_info.pPoolSizes = pool_sizes;
    pool_info.maxSets = 5 + k_max_material_count;
    pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;

    if (vkCreateDescriptorPool(device, &pool_info, nullptr, &descriptor_pool) !=
        VK_SUCCESS) {
        VAIN_ERROR("failed to create descriptor pool");
    }
}

void VulkanContext::createSyncPrimitives() {
    VkSemaphoreCreateInfo semaphore_create_info{};
    semaphore_create_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo fence_create_info{};
    fence_create_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fence_create_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    for (uint32_t i = 0; i < k_max_frames_in_flight; i++) {
        if (vkCreateSemaphore(
                device,
                &semaphore_create_info,
                nullptr,
                &image_available_for_render_semaphores[i]
            ) != VK_SUCCESS ||
            vkCreateSemaphore(
                device,
                &semaphore_create_info,
                nullptr,
                &image_finished_for_presentation_semaphores[i]
            ) != VK_SUCCESS ||
            vkCreateFence(
                device, &fence_create_info, nullptr, &is_frame_in_flight_fences[i]
            ) != VK_SUCCESS) {
            VAIN_ERROR("failed to create semaphore & fence");
        }
    }
}

void VulkanContext::createSwapchain() {
    SwapChainSupportDetails swapchain_support_details(physical_device, surface);

    VkSurfaceFormatKHR chosen_surface_format = swapchain_support_details.formats[0];
    for (const auto &surface_format : swapchain_support_details.formats) {
        if (surface_format.format == VK_FORMAT_B8G8R8A8_SRGB &&
            surface_format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            chosen_surface_format = surface_format;
            break;
        }
    }

    VkPresentModeKHR chosen_present_mode = VK_PRESENT_MODE_FIFO_KHR;
    // for (VkPresentModeKHR present_mode : swapchain_support_details.present_modes) {
    //   if (present_mode == VK_PRESENT_MODE_MAILBOX_KHR) {
    //       chosen_present_mode = VK_PRESENT_MODE_MAILBOX_KHR;
    //       break;
    //    }
    // }

    VkExtent2D chosen_extent;
    if (swapchain_support_details.capabilities.currentExtent.width != UINT32_MAX) {
        chosen_extent = swapchain_support_details.capabilities.currentExtent;
    } else {
        int width, height;
        glfwGetFramebufferSize(window, &width, &height);

        chosen_extent.width = static_cast<uint32_t>(width);
        chosen_extent.height = static_cast<uint32_t>(height);

        chosen_extent.width = std::clamp(
            chosen_extent.width,
            swapchain_support_details.capabilities.minImageExtent.width,
            swapchain_support_details.capabilities.maxImageExtent.width
        );
        chosen_extent.height = std::clamp(
            chosen_extent.height,
            swapchain_support_details.capabilities.minImageExtent.height,
            swapchain_support_details.capabilities.maxImageExtent.height
        );
    }

    uint32_t image_count = swapchain_support_details.capabilities.minImageCount + 1;
    if (swapchain_support_details.capabilities.maxImageCount > 0 &&
        image_count > swapchain_support_details.capabilities.maxImageCount) {
        image_count = swapchain_support_details.capabilities.maxImageCount;
    }

    VkSwapchainCreateInfoKHR swapchain_create_info{};
    swapchain_create_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    swapchain_create_info.surface = surface;
    swapchain_create_info.minImageCount = image_count;
    swapchain_create_info.imageFormat = chosen_surface_format.format;
    swapchain_create_info.imageColorSpace = chosen_surface_format.colorSpace;
    swapchain_create_info.imageExtent = chosen_extent;
    swapchain_create_info.imageArrayLayers = 1;
    swapchain_create_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    uint32_t queue_family_indices[] = {
        queue_indices.graphics_family.value(), queue_indices.present_family.value()
    };

    if (queue_family_indices[0] != queue_family_indices[1]) {
        swapchain_create_info.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        swapchain_create_info.queueFamilyIndexCount = 2;
        swapchain_create_info.pQueueFamilyIndices = queue_family_indices;
    } else {
        swapchain_create_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }

    swapchain_create_info.preTransform =
        swapchain_support_details.capabilities.currentTransform;
    swapchain_create_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    swapchain_create_info.presentMode = chosen_present_mode;
    swapchain_create_info.clipped = VK_TRUE;

    swapchain_create_info.oldSwapchain = VK_NULL_HANDLE;

    if (vkCreateSwapchainKHR(device, &swapchain_create_info, nullptr, &swapchain) !=
        VK_SUCCESS) {
        VAIN_ERROR("failed to create swapchain");
    }

    vkGetSwapchainImagesKHR(device, swapchain, &image_count, nullptr);
    swapchain_images.resize(image_count);
    vkGetSwapchainImagesKHR(device, swapchain, &image_count, swapchain_images.data());

    swapchain_image_format = chosen_surface_format.format;
    swapchain_extent.height = chosen_extent.height;
    swapchain_extent.width = chosen_extent.width;
}

void VulkanContext::createSwapchainImageViews() {
    swapchain_image_views.resize(swapchain_images.size());
    for (size_t i = 0; i < swapchain_image_views.size(); ++i) {
        swapchain_image_views[i] = createImageView(
            device,
            swapchain_images[i],
            swapchain_image_format,
            VK_IMAGE_ASPECT_COLOR_BIT,
            VK_IMAGE_VIEW_TYPE_2D,
            1,
            1
        );
    }
}

void VulkanContext::createDepthImageAndView() {
    depth_image_format = findDepthFormat(physical_device);
    createImage(
        physical_device,
        device,
        swapchain_extent.width,
        swapchain_extent.height,
        depth_image_format,
        VK_IMAGE_TILING_OPTIMAL,
        VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT |
            VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        0,
        1,
        1,
        depth_image,
        depth_image_memory
    );

    depth_image_view = createImageView(
        device,
        depth_image,
        depth_image_format,
        VK_IMAGE_ASPECT_DEPTH_BIT,
        VK_IMAGE_VIEW_TYPE_2D,
        1,
        1
    );
}

void VulkanContext::createAssetAllocator() {
    VmaVulkanFunctions vulkan_functions{};
    vulkan_functions.vkGetInstanceProcAddr = &vkGetInstanceProcAddr;
    vulkan_functions.vkGetDeviceProcAddr = &vkGetDeviceProcAddr;

    VmaAllocatorCreateInfo allocator_create_info{};
    allocator_create_info.vulkanApiVersion = s_vulkan_api_version;
    allocator_create_info.physicalDevice = physical_device;
    allocator_create_info.device = device;
    allocator_create_info.instance = instance;
    allocator_create_info.pVulkanFunctions = &vulkan_functions;

    VkResult res = vmaCreateAllocator(&allocator_create_info, &assets_allocator);
    if (res != VK_SUCCESS) {
        VAIN_ERROR("failed to create vma allocator");
    }
}

void VulkanContext::clearSwapchain() {
    for (auto imageview : swapchain_image_views) {
        vkDestroyImageView(device, imageview, nullptr);
    }
    vkDestroySwapchainKHR(device, swapchain, nullptr);
}

void VulkanContext::recreateSwapchain() {
    int width = 0;
    int height = 0;
    glfwGetFramebufferSize(window, &width, &height);
    while (width == 0 || height == 0) {
        glfwGetFramebufferSize(window, &width, &height);
        glfwWaitEvents();
    }
    VkResult res_wait_for_fences = waitForFences(
        device, k_max_frames_in_flight, is_frame_in_flight_fences, VK_TRUE, UINT64_MAX
    );
    if (res_wait_for_fences != VK_SUCCESS) {
        VAIN_ERROR("failed to wait for fence");
        return;
    }

    vkDestroyImageView(device, depth_image_view, nullptr);
    vkDestroyImage(device, depth_image, nullptr);
    vkFreeMemory(device, depth_image_memory, nullptr);

    clearSwapchain();

    createSwapchain();
    createSwapchainImageViews();
    createDepthImageAndView();
}

}  // namespace Vain