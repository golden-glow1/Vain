#include "render_type.h"

namespace Vain {

std::array<VkVertexInputBindingDescription, 1> MeshVertex::getBindingDescriptions() {
    std::array<VkVertexInputBindingDescription, 1> binding_descriptions{};

    binding_descriptions[0].binding = 0;
    binding_descriptions[0].stride = sizeof(MeshVertex);
    binding_descriptions[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    return binding_descriptions;
}

std::array<VkVertexInputAttributeDescription, 4> MeshVertex::getAttributeDescriptions() {
    std::array<VkVertexInputAttributeDescription, 4> attribute_descriptions{};

    attribute_descriptions[0].location = 0;
    attribute_descriptions[0].binding = 0;
    attribute_descriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
    attribute_descriptions[0].offset = offsetof(MeshVertex, position);

    attribute_descriptions[1].location = 1;
    attribute_descriptions[1].binding = 0;
    attribute_descriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
    attribute_descriptions[1].offset = offsetof(MeshVertex, normal);

    attribute_descriptions[2].location = 2;
    attribute_descriptions[2].binding = 0;
    attribute_descriptions[2].format = VK_FORMAT_R32G32B32_SFLOAT;
    attribute_descriptions[2].offset = offsetof(MeshVertex, tangent);

    attribute_descriptions[3].location = 3;
    attribute_descriptions[3].binding = 0;
    attribute_descriptions[3].format = VK_FORMAT_R32G32_SFLOAT;
    attribute_descriptions[3].offset = offsetof(MeshVertex, texcoord);

    return attribute_descriptions;
}

}  // namespace Vain