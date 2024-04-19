#include "vulkan_utils.h"

#include "core/base/macro.h"
#include "vulkan_context.h"

namespace Vain {

uint32_t findMemoryType(
    VkPhysicalDevice physical_device,
    uint32_t type_filter,
    VkMemoryPropertyFlags properties_flag
) {
    VkPhysicalDeviceMemoryProperties physical_device_memory_properties;
    vkGetPhysicalDeviceMemoryProperties(
        physical_device, &physical_device_memory_properties
    );
    for (uint32_t i = 0; i < physical_device_memory_properties.memoryTypeCount; i++) {
        if (type_filter & (1 << i) &&
            (physical_device_memory_properties.memoryTypes[i].propertyFlags &
             properties_flag) == properties_flag) {
            return i;
        }
    }
    VAIN_ERROR("findMemoryType error");
    return 0;
}

VkShaderModule createShaderModule(
    VkDevice device, const std::vector<uint8_t> &shader_code
) {
    VkShaderModuleCreateInfo shader_module_create_info{};
    shader_module_create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    shader_module_create_info.codeSize = shader_code.size();
    shader_module_create_info.pCode =
        reinterpret_cast<const uint32_t *>(shader_code.data());

    VkShaderModule shader_module;
    if (vkCreateShaderModule(
            device, &shader_module_create_info, nullptr, &shader_module
        ) != VK_SUCCESS) {
        return VK_NULL_HANDLE;
    }
    return shader_module;
}

void createBuffer(
    VkPhysicalDevice physical_device,
    VkDevice device,
    VkDeviceSize size,
    VkBufferUsageFlags usage,
    VkMemoryPropertyFlags memory_property_flags,
    VkBuffer &buffer,
    VkDeviceMemory &buffer_memory
) {
    VkBufferCreateInfo buffer_create_info{};
    buffer_create_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    buffer_create_info.size = size;
    buffer_create_info.usage = usage;
    buffer_create_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateBuffer(device, &buffer_create_info, nullptr, &buffer) != VK_SUCCESS) {
        VAIN_ERROR("failed to create buffer");
        return;
    }

    VkMemoryRequirements mem_requirements;
    vkGetBufferMemoryRequirements(device, buffer, &mem_requirements);
    VkMemoryAllocateInfo alloc_info{};
    alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    alloc_info.allocationSize = mem_requirements.size;
    alloc_info.memoryTypeIndex = findMemoryType(
        physical_device, mem_requirements.memoryTypeBits, memory_property_flags
    );

    if (vkAllocateMemory(device, &alloc_info, nullptr, &buffer_memory) != VK_SUCCESS) {
        VAIN_ERROR("failed to allocate buffer memory");
        return;
    }

    vkBindBufferMemory(device, buffer, buffer_memory, 0);
}

void copyBuffer(
    VulkanContext *ctx,
    VkBuffer srcBuffer,
    VkBuffer dstBuffer,
    VkDeviceSize srcOffset,
    VkDeviceSize dstOffset,
    VkDeviceSize size
) {
    VkCommandBuffer command_buffer = ctx->beginSingleTimeCommands();

    VkBufferCopy copyRegion = {srcOffset, dstOffset, size};
    vkCmdCopyBuffer(command_buffer, srcBuffer, dstBuffer, 1, &copyRegion);

    ctx->endSingleTimeCommands(command_buffer);
}

void copyBufferToImage(
    VulkanContext *ctx,
    VkBuffer buffer,
    VkImage image,
    uint32_t width,
    uint32_t height,
    uint32_t layer_count
) {
    VkCommandBuffer command_buffer = ctx->beginSingleTimeCommands();

    VkBufferImageCopy region{};

    region.bufferOffset = 0;
    region.bufferRowLength = 0;
    region.bufferImageHeight = 0;
    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.mipLevel = 0;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount = layer_count;
    region.imageOffset = {0, 0, 0};
    region.imageExtent = {width, height, 1};

    vkCmdCopyBufferToImage(
        command_buffer, buffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region
    );

    ctx->endSingleTimeCommands(command_buffer);
}

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

) {
    VkImageCreateInfo image_create_info{};
    image_create_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    image_create_info.flags = image_create_flags;
    image_create_info.imageType = VK_IMAGE_TYPE_2D;
    image_create_info.extent.width = image_width;
    image_create_info.extent.height = image_height;
    image_create_info.extent.depth = 1;
    image_create_info.mipLevels = mip_levels;
    image_create_info.arrayLayers = array_layers;
    image_create_info.format = format;
    image_create_info.tiling = image_tiling;
    image_create_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    image_create_info.usage = image_usage_flags;
    image_create_info.samples = VK_SAMPLE_COUNT_1_BIT;
    image_create_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateImage(device, &image_create_info, nullptr, &image) != VK_SUCCESS) {
        VAIN_ERROR("failed to create image");
        return;
    }

    VkMemoryRequirements mem_requirements;
    vkGetImageMemoryRequirements(device, image, &mem_requirements);
    VkMemoryAllocateInfo alloc_info{};
    alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    alloc_info.allocationSize = mem_requirements.size;
    alloc_info.memoryTypeIndex = findMemoryType(
        physical_device, mem_requirements.memoryTypeBits, memory_property_flags
    );

    if (vkAllocateMemory(device, &alloc_info, nullptr, &memory) != VK_SUCCESS) {
        VAIN_ERROR("failed to allocate image memory");
        return;
    }

    vkBindImageMemory(device, image, memory, 0);
}

VkImageView createImageView(
    VkDevice device,
    VkImage image,
    VkFormat format,
    VkImageAspectFlags image_aspect_flags,
    VkImageViewType view_type,
    uint32_t layout_count,
    uint32_t mip_levels
) {
    VkImageViewCreateInfo image_view_create_info{};
    image_view_create_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    image_view_create_info.image = image;
    image_view_create_info.viewType = view_type;
    image_view_create_info.format = format;
    image_view_create_info.subresourceRange.aspectMask = image_aspect_flags;
    image_view_create_info.subresourceRange.baseMipLevel = 0;
    image_view_create_info.subresourceRange.levelCount = mip_levels;
    image_view_create_info.subresourceRange.baseArrayLayer = 0;
    image_view_create_info.subresourceRange.layerCount = layout_count;

    VkImageView image_view{};
    if (vkCreateImageView(device, &image_view_create_info, nullptr, &image_view) !=
        VK_SUCCESS) {
        VAIN_ERROR("failed to create image view");
    }
    return image_view;
}

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
) {
    if (!texture_image_pixels) {
        return;
    }

    VkDeviceSize texture_byte_size;

    switch (texture_image_format) {
    case VK_FORMAT_R8G8B8_UNORM:
    case VK_FORMAT_R8G8B8_SRGB:
        texture_byte_size = texture_image_width * texture_image_height * 3;
        break;
    case VK_FORMAT_R8G8B8A8_UNORM:
    case VK_FORMAT_R8G8B8A8_SRGB:
        texture_byte_size = texture_image_width * texture_image_height * 4;
        break;
    case VK_FORMAT_R32G32_SFLOAT:
        texture_byte_size = texture_image_width * texture_image_height * 4 * 2;
        break;
    case VK_FORMAT_R32G32B32_SFLOAT:
        texture_byte_size = texture_image_width * texture_image_height * 4 * 3;
        break;
    case VK_FORMAT_R32G32B32A32_SFLOAT:
        texture_byte_size = texture_image_width * texture_image_height * 4 * 4;
        break;
    default:
        texture_byte_size = VkDeviceSize(-1);
        VAIN_ERROR("invalid texture image format");
        return;
    }

    VkBuffer inefficient_staging_buffer;
    VkDeviceMemory inefficient_staging_buffer_memory;
    createBuffer(
        ctx->physical_device,
        ctx->device,
        texture_byte_size,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        inefficient_staging_buffer,
        inefficient_staging_buffer_memory
    );
    void *data = nullptr;
    vkMapMemory(
        ctx->device, inefficient_staging_buffer_memory, 0, texture_byte_size, 0, &data
    );
    memcpy(data, texture_image_pixels, texture_byte_size);
    vkUnmapMemory(ctx->device, inefficient_staging_buffer_memory);

    if (mip_levels == 0) {
        mip_levels = floor(log2(std::max(texture_image_width, texture_image_height))) + 1;
    }

    VkImageCreateInfo image_create_info{};
    image_create_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    image_create_info.imageType = VK_IMAGE_TYPE_2D;
    image_create_info.extent.width = texture_image_width;
    image_create_info.extent.height = texture_image_height;
    image_create_info.extent.depth = 1;
    image_create_info.mipLevels = mip_levels;
    image_create_info.arrayLayers = 1;
    image_create_info.format = texture_image_format;
    image_create_info.tiling = VK_IMAGE_TILING_OPTIMAL;
    image_create_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    image_create_info.usage = VK_IMAGE_USAGE_SAMPLED_BIT |
                              VK_IMAGE_USAGE_TRANSFER_DST_BIT |
                              VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    image_create_info.samples = VK_SAMPLE_COUNT_1_BIT;
    image_create_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VmaAllocationCreateInfo alloc_info = {};
    alloc_info.usage = VMA_MEMORY_USAGE_GPU_ONLY;

    vmaCreateImage(
        ctx->assets_allocator,
        &image_create_info,
        &alloc_info,
        &image,
        &image_allocation,
        nullptr
    );

    transitionImageLayout(
        ctx,
        image,
        VK_IMAGE_LAYOUT_UNDEFINED,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        1,
        mip_levels,
        VK_IMAGE_ASPECT_COLOR_BIT
    );
    copyBufferToImage(
        ctx,
        inefficient_staging_buffer,
        image,
        texture_image_width,
        texture_image_height,
        1
    );

    vkDestroyBuffer(ctx->device, inefficient_staging_buffer, nullptr);
    vkFreeMemory(ctx->device, inefficient_staging_buffer_memory, nullptr);

    generateTextureMipMaps(
        ctx,
        image,
        texture_image_format,
        texture_image_width,
        texture_image_height,
        1,
        mip_levels
    );

    image_view = createImageView(
        ctx->device,
        image,
        texture_image_format,
        VK_IMAGE_ASPECT_COLOR_BIT,
        VK_IMAGE_VIEW_TYPE_2D,
        1,
        mip_levels
    );
}

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
) {
    VkDeviceSize texture_layer_byte_size;
    VkDeviceSize cube_byte_size;

    switch (texture_image_format) {
    case VK_FORMAT_R8G8B8_UNORM:
    case VK_FORMAT_R8G8B8_SRGB:
        texture_layer_byte_size = texture_image_width * texture_image_height * 3;
        break;
    case VK_FORMAT_R8G8B8A8_UNORM:
    case VK_FORMAT_R8G8B8A8_SRGB:
        texture_layer_byte_size = texture_image_width * texture_image_height * 4;
        break;
    case VK_FORMAT_R32G32_SFLOAT:
        texture_layer_byte_size = texture_image_width * texture_image_height * 4 * 2;
        break;
    case VK_FORMAT_R32G32B32_SFLOAT:
        texture_layer_byte_size = texture_image_width * texture_image_height * 4 * 3;
        break;
    case VK_FORMAT_R32G32B32A32_SFLOAT:
        texture_layer_byte_size = texture_image_width * texture_image_height * 4 * 4;
        break;
    default:
        texture_layer_byte_size = VkDeviceSize(-1);
        VAIN_ERROR("invalid texture image format");
        return;
    }

    cube_byte_size = 6 * texture_layer_byte_size;

    if (mip_levels == 0) {
        mip_levels = floor(log2(std::max(texture_image_width, texture_image_height))) + 1;
    }

    VkImageCreateInfo image_create_info{};
    image_create_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    image_create_info.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
    image_create_info.imageType = VK_IMAGE_TYPE_2D;
    image_create_info.extent.width = static_cast<uint32_t>(texture_image_width);
    image_create_info.extent.height = static_cast<uint32_t>(texture_image_height);
    image_create_info.extent.depth = 1;
    image_create_info.mipLevels = mip_levels;
    image_create_info.arrayLayers = 6;
    image_create_info.format = texture_image_format;
    image_create_info.tiling = VK_IMAGE_TILING_OPTIMAL;
    image_create_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    image_create_info.usage = VK_IMAGE_USAGE_SAMPLED_BIT |
                              VK_IMAGE_USAGE_TRANSFER_DST_BIT |
                              VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    image_create_info.samples = VK_SAMPLE_COUNT_1_BIT;
    image_create_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VmaAllocationCreateInfo alloc_info{};
    alloc_info.usage = VMA_MEMORY_USAGE_GPU_ONLY;

    vmaCreateImage(
        ctx->assets_allocator,
        &image_create_info,
        &alloc_info,
        &image,
        &image_allocation,
        nullptr
    );

    VkBuffer inefficient_staging_buffer;
    VkDeviceMemory inefficient_staging_buffer_memory;
    createBuffer(
        ctx->physical_device,
        ctx->device,
        cube_byte_size,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        inefficient_staging_buffer,
        inefficient_staging_buffer_memory
    );

    void *data = nullptr;
    vkMapMemory(
        ctx->device, inefficient_staging_buffer_memory, 0, cube_byte_size, 0, &data
    );
    for (int i = 0; i < 6; i++) {
        memcpy(
            (void *)(static_cast<char *>(data) + texture_layer_byte_size * i),
            texture_image_pixels[i],
            static_cast<size_t>(texture_layer_byte_size)
        );
    }
    vkUnmapMemory(ctx->device, inefficient_staging_buffer_memory);

    transitionImageLayout(
        ctx,
        image,
        VK_IMAGE_LAYOUT_UNDEFINED,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        6,
        mip_levels,
        VK_IMAGE_ASPECT_COLOR_BIT
    );

    copyBufferToImage(
        ctx,
        inefficient_staging_buffer,
        image,
        texture_image_width,
        texture_image_height,
        6
    );

    vkDestroyBuffer(ctx->device, inefficient_staging_buffer, nullptr);
    vkFreeMemory(ctx->device, inefficient_staging_buffer_memory, nullptr);

    generateTextureMipMaps(
        ctx,
        image,
        texture_image_format,
        texture_image_width,
        texture_image_height,
        6,
        mip_levels
    );

    image_view = createImageView(
        ctx->device,
        image,
        texture_image_format,
        VK_IMAGE_ASPECT_COLOR_BIT,
        VK_IMAGE_VIEW_TYPE_CUBE,
        6,
        mip_levels
    );
}

void generateTextureMipMaps(
    VulkanContext *ctx,
    VkImage image,
    VkFormat image_format,
    uint32_t texture_width,
    uint32_t texture_height,
    uint32_t layers,
    uint32_t mip_levels
) {
    VkFormatProperties format_properties;
    vkGetPhysicalDeviceFormatProperties(
        ctx->physical_device, image_format, &format_properties
    );
    if (!(format_properties.optimalTilingFeatures &
          VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT)) {
        VAIN_ERROR("linear bliting not supported");
        return;
    }

    VkCommandBuffer command_buffer = ctx->beginSingleTimeCommands();

    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.image = image;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = layers;
    barrier.subresourceRange.levelCount = 1;

    int32_t mipwidth = texture_width;
    int32_t mipheight = texture_height;

    for (uint32_t i = 1; i < mip_levels; i++) {
        barrier.subresourceRange.baseMipLevel = i - 1;
        barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

        vkCmdPipelineBarrier(
            command_buffer,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            0,
            0,
            nullptr,
            0,
            nullptr,
            1,
            &barrier
        );

        VkImageBlit blit{};
        blit.srcOffsets[0] = {0, 0, 0};
        blit.srcOffsets[1] = {mipwidth, mipheight, 1};
        blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        blit.srcSubresource.mipLevel = i - 1;
        blit.srcSubresource.baseArrayLayer = 0;
        blit.srcSubresource.layerCount = layers;

        blit.dstOffsets[0] = {0, 0, 0};
        blit.dstOffsets[1] = {
            mipwidth > 1 ? mipwidth / 2 : 1, mipheight > 1 ? mipheight / 2 : 1, 1
        };
        blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        blit.dstSubresource.mipLevel = i;
        blit.dstSubresource.baseArrayLayer = 0;
        blit.dstSubresource.layerCount = layers;

        vkCmdBlitImage(
            command_buffer,
            image,
            VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            image,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            1,
            &blit,
            VK_FILTER_LINEAR
        );

        barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        vkCmdPipelineBarrier(
            command_buffer,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            0,
            0,
            nullptr,
            0,
            nullptr,
            1,
            &barrier
        );

        if (mipwidth > 1) mipwidth /= 2;
        if (mipheight > 1) mipheight /= 2;
    }

    barrier.subresourceRange.baseMipLevel = mip_levels - 1;
    barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    vkCmdPipelineBarrier(
        command_buffer,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
        0,
        0,
        nullptr,
        0,
        nullptr,
        1,
        &barrier
    );

    ctx->endSingleTimeCommands(command_buffer);
}

void transitionImageLayout(
    VulkanContext *ctx,
    VkImage image,
    VkImageLayout old_layout,
    VkImageLayout new_layout,
    uint32_t layer_count,
    uint32_t mip_levels,
    VkImageAspectFlags aspect_mask_bits
) {
    VkCommandBuffer command_buffer = ctx->beginSingleTimeCommands();

    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = old_layout;
    barrier.newLayout = new_layout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image;
    barrier.subresourceRange.aspectMask = aspect_mask_bits;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = mip_levels;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = layer_count;

    VkPipelineStageFlags source_stage;
    VkPipelineStageFlags destination_stage;

    if (old_layout == VK_IMAGE_LAYOUT_UNDEFINED &&
        new_layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

        source_stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        destination_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    } else if (old_layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && new_layout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

        source_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        destination_stage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    } else if (old_layout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL && new_layout == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL) {
        // for getGuidAndDepthOfMouseClickOnRenderSceneForUI() get depthimage

        barrier.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

        source_stage = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
        destination_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    } else if (old_layout == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL && new_layout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL) {
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        barrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

        source_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        destination_stage = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    } else if (old_layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && new_layout == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL) {
        // for generating mipmapped image

        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

        source_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        destination_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    } else {
        VAIN_ERROR("unsupported layout transition!");
        return;
    }

    vkCmdPipelineBarrier(
        command_buffer,
        source_stage,
        destination_stage,
        0,
        0,
        nullptr,
        0,
        nullptr,
        1,
        &barrier
    );

    ctx->endSingleTimeCommands(command_buffer);
}

};  // namespace Vain