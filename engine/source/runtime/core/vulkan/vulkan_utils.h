#pragma once

#include <vk_mem_alloc.h>
#include <vulkan/vulkan.h>

#include <array>
#include <vector>

namespace Vain {

class VulkanContext;

uint32_t findMemoryType(
    VkPhysicalDevice physical_device,
    uint32_t type_filter,
    VkMemoryPropertyFlags properties_flag
);

VkShaderModule createShaderModule(
    VkDevice device, const std::vector<uint8_t> &shader_code
);

void createBuffer(
    VkPhysicalDevice physical_device,
    VkDevice device,
    VkDeviceSize size,
    VkBufferUsageFlags usage,
    VkMemoryPropertyFlags properties,
    VkBuffer &buffer,
    VkDeviceMemory &buffer_memory
);

void copyBuffer(
    VulkanContext *ctx,
    VkBuffer srcBuffer,
    VkBuffer dstBuffer,
    VkDeviceSize srcOffset,
    VkDeviceSize dstOffset,
    VkDeviceSize size
);

void copyBufferToImage(
    VulkanContext *ctx,
    VkBuffer buffer,
    VkImage image,
    uint32_t width,
    uint32_t height,
    uint32_t layer_count
);

void createImage(
    VkPhysicalDevice physical_device,
    VkDevice device,
    uint32_t image_width,
    uint32_t image_height,
    VkFormat format,
    VkImageTiling image_tiling,
    VkImageUsageFlags image_usage_flags,
    VkMemoryPropertyFlags memory_property_flags,
    VkImageCreateFlags image_create_flags,
    uint32_t array_layers,
    uint32_t mip_levels,
    VkImage &image,
    VkDeviceMemory &memory
);

VkImageView createImageView(
    VkDevice device,
    VkImage image,
    VkFormat format,
    VkImageAspectFlags image_aspect_flags,
    VkImageViewType view_type,
    uint32_t layout_count,
    uint32_t mip_levels
);

void createTexture(
    VulkanContext *ctx,
    uint32_t texture_image_width,
    uint32_t texture_image_height,
    void *texture_image_pixels,
    VkFormat texture_image_format,
    uint32_t mip_levels,
    VkImage &image,
    VkImageView &image_view,
    VmaAllocation &image_allocation
);

void createCubeMap(
    VulkanContext *ctx,
    uint32_t texture_image_width,
    uint32_t texture_image_height,
    std::array<void *, 6> texture_image_pixels,
    VkFormat texture_image_format,
    uint32_t mip_levels,
    VkImage &image,
    VkImageView &image_view,
    VmaAllocation &image_allocation
);

void generateTextureMipMaps(
    VulkanContext *ctx,
    VkImage image,
    VkFormat image_format,
    uint32_t texture_width,
    uint32_t texture_height,
    uint32_t layers,
    uint32_t mip_levels
);

void transitionImageLayout(
    VulkanContext *ctx,
    VkImage image,
    VkImageLayout old_layout,
    VkImageLayout new_layout,
    uint32_t layer_count,
    uint32_t mip_levels,
    VkImageAspectFlags aspect_mask_bits
);

}  // namespace Vain