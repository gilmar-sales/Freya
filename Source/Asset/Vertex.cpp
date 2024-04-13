#include "Asset/Vertex.hpp"

namespace FREYA_NAMESPACE
{

    vk::VertexInputBindingDescription Vertex::GetBindingDescription()
    {
        static auto bindingDescription =
            vk::VertexInputBindingDescription()
                .setBinding(0)
                .setStride(sizeof(Vertex))
                .setInputRate(vk::VertexInputRate::eVertex);

        return bindingDescription;
    }

    std::array<vk::VertexInputAttributeDescription, 2> Vertex::GetAttributesDescription()
    {
        static auto attributesDescription =
            std::array<vk::VertexInputAttributeDescription, 2> {
                vk::VertexInputAttributeDescription()
                    .setBinding(0)
                    .setLocation(0)
                    .setFormat(vk::Format::eR32G32B32Sfloat)
                    .setOffset(offsetof(Vertex, position)),
                vk::VertexInputAttributeDescription()
                    .setBinding(0)
                    .setLocation(1)
                    .setFormat(vk::Format::eR32G32B32Sfloat)
                    .setOffset(offsetof(Vertex, color))
            };

        return attributesDescription;
    }
} // namespace FREYA_NAMESPACE
