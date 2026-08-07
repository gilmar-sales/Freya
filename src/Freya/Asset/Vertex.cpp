#include "Freya/Asset/Vertex.hpp"
#include "Freya/Asset/InstanceTransform.hpp"

namespace FREYA_NAMESPACE
{

    std::vector<vk::VertexInputBindingDescription> Vertex::
        GetBindingDescription()
    {
        static auto bindingDescription = std::vector {
            vk::VertexInputBindingDescription()
                .setBinding(0)
                .setStride(sizeof(Vertex))
                .setInputRate(vk::VertexInputRate::eVertex),
            vk::VertexInputBindingDescription()
                .setBinding(1)
                .setStride(sizeof(InstanceTransform))
                .setInputRate(vk::VertexInputRate::eInstance)
        };

        return bindingDescription;
    }

    std::vector<vk::VertexInputAttributeDescription> Vertex::
        GetAttributesDescription()
    {
        static auto attributesDescription = std::vector {
            vk::VertexInputAttributeDescription()
                .setBinding(0)
                .setLocation(0)
                .setFormat(vk::Format::eR32G32B32Sfloat)
                .setOffset(offsetof(Vertex, position)),
            vk::VertexInputAttributeDescription()
                .setBinding(0)
                .setLocation(1)
                .setFormat(vk::Format::eR32G32B32Sfloat)
                .setOffset(offsetof(Vertex, color)),
            vk::VertexInputAttributeDescription()
                .setBinding(0)
                .setLocation(2)
                .setFormat(vk::Format::eR32G32B32Sfloat)
                .setOffset(offsetof(Vertex, normal)),
            vk::VertexInputAttributeDescription()
                .setBinding(0)
                .setLocation(3)
                .setFormat(vk::Format::eR32G32B32Sfloat)
                .setOffset(offsetof(Vertex, tangent)),
            vk::VertexInputAttributeDescription()
                .setBinding(0)
                .setLocation(4)
                .setFormat(vk::Format::eR32G32Sfloat)
                .setOffset(offsetof(Vertex, texCoord)),
            vk::VertexInputAttributeDescription()
                .setBinding(1)
                .setLocation(5)
                .setFormat(vk::Format::eR32G32B32A32Sfloat)
                .setOffset(offsetof(InstanceTransform, model)),
            vk::VertexInputAttributeDescription()
                .setBinding(1)
                .setLocation(6)
                .setFormat(vk::Format::eR32G32B32A32Sfloat)
                .setOffset(
                    offsetof(InstanceTransform, model) + sizeof(glm::vec4)),
            vk::VertexInputAttributeDescription()
                .setBinding(1)
                .setLocation(7)
                .setFormat(vk::Format::eR32G32B32A32Sfloat)
                .setOffset(
                    offsetof(InstanceTransform, model) + sizeof(glm::vec4) * 2),
            vk::VertexInputAttributeDescription()
                .setBinding(1)
                .setLocation(8)
                .setFormat(vk::Format::eR32G32B32A32Sfloat)
                .setOffset(
                    offsetof(InstanceTransform, model) + sizeof(glm::vec4) * 3),
            vk::VertexInputAttributeDescription()
                .setBinding(1)
                .setLocation(9)
                .setFormat(vk::Format::eR32G32B32A32Sfloat)
                .setOffset(offsetof(InstanceTransform, prevModel)),
            vk::VertexInputAttributeDescription()
                .setBinding(1)
                .setLocation(10)
                .setFormat(vk::Format::eR32G32B32A32Sfloat)
                .setOffset(
                    offsetof(InstanceTransform, prevModel) + sizeof(glm::vec4)),
            vk::VertexInputAttributeDescription()
                .setBinding(1)
                .setLocation(11)
                .setFormat(vk::Format::eR32G32B32A32Sfloat)
                .setOffset(offsetof(InstanceTransform, prevModel) +
                           sizeof(glm::vec4) * 2),
            vk::VertexInputAttributeDescription()
                .setBinding(1)
                .setLocation(12)
                .setFormat(vk::Format::eR32G32B32A32Sfloat)
                .setOffset(offsetof(InstanceTransform, prevModel) +
                           sizeof(glm::vec4) * 3),
        };

        return attributesDescription;
    }
} // namespace FREYA_NAMESPACE
