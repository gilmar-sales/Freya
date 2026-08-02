#pragma once

#include "Freya/Core/Buffer.hpp"
#include "Freya/Core/Device.hpp"
#include "Freya/Core/UniformBuffer.hpp"

#include <cmath>

namespace FREYA_NAMESPACE
{
    /**
     * @brief Light source data structure for the lighting system.
     *
     * Represents a single light with position, color, and type-specific
     * parameters. Packed into LightUniformBuffer as SoA (std140): type lives
     * in lightPositions[i].w; spot cutoffs are stored as cosines.
     */
    struct Light
    {
        glm::vec3 position =
            glm::vec3(0.0f);     ///< World position (for point/spot lights)
        float     type   = 0.0f; ///< LightType (0=Point, 1=Directional, 2=Spot)
        glm::vec3 color  = glm::vec3(1.0f); ///< RGB light color
        float     radius = 10.0f; ///< Attenuation radius (point/spot lights)
        glm::vec3 direction = glm::vec3(
            0.0f, -1.0f, 0.0f);   ///< Direction (for directional/spot lights)
        float innerCutoff = 0.9f; ///< Inner spotlight cutoff cosine (spot only)
        float outerCutoff = 0.8f; ///< Outer spotlight cutoff cosine (spot only)
        float intensity   = 1.0f; ///< Light intensity multiplier
    };

    /**
     * @brief Creates a point light.
     */
    inline Light MakePointLight(const glm::vec3& position,
                                const glm::vec3& color,
                                float            radius,
                                float            intensity = 1.0f)
    {
        Light light {};
        light.position  = position;
        light.type      = static_cast<float>(LightType::Point);
        light.color     = color;
        light.radius    = radius;
        light.intensity = intensity;
        return light;
    }

    /**
     * @brief Creates a directional light. Direction is normalized.
     */
    inline Light MakeDirectionalLight(const glm::vec3& direction,
                                      const glm::vec3& color,
                                      float            intensity = 1.0f)
    {
        Light light {};
        light.type      = static_cast<float>(LightType::Directional);
        light.color     = color;
        light.direction = glm::normalize(direction);
        light.intensity = intensity;
        return light;
    }

    /**
     * @brief Creates a spot light.
     * @param innerAngleRad Inner cone half-angle in radians (full intensity).
     * @param outerAngleRad Outer cone half-angle in radians (falloff end).
     *        Stored as cosines in Light::innerCutoff / outerCutoff.
     */
    inline Light MakeSpotLight(const glm::vec3& position,
                               const glm::vec3& direction,
                               const glm::vec3& color,
                               float            radius,
                               float            innerAngleRad,
                               float            outerAngleRad,
                               float            intensity = 1.0f)
    {
        Light light {};
        light.position    = position;
        light.type        = static_cast<float>(LightType::Spot);
        light.color       = color;
        light.radius      = radius;
        light.direction   = glm::normalize(direction);
        light.innerCutoff = std::cos(innerAngleRad);
        light.outerCutoff = std::cos(outerAngleRad);
        light.intensity   = intensity;
        return light;
    }

    /**
     * @brief Service for managing lights and updating the light uniform buffer.
     *
     * Manages a collection of lights and provides the GPU buffer with light
     * data for both forward and deferred rendering pipelines.
     */
    class LightService
    {
      public:
        /**
         * @brief Constructs a LightService.
         * @param device     Vulkan device reference
         * @param frameCount Number of frames (for ring-buffer descriptor sets)
         * @param maxLights  Maximum number of lights (default: MAX_LIGHTS)
         */
        LightService(const skr::Arc<Device>& device,
                     std::uint32_t           frameCount,
                     std::uint32_t           maxLights = MAX_LIGHTS);

        ~LightService();

        // Non-copyable
        LightService(const LightService&)            = delete;
        LightService& operator=(const LightService&) = delete;

        // Movable
        LightService(LightService&&) noexcept;
        LightService& operator=(LightService&&) noexcept;

        /**
         * @brief Adds a light to the service.
         * @param light Light to add
         * @return Index of the added light, or -1 if max lights reached
         */
        std::int32_t AddLight(const Light& light);

        /**
         * @brief Removes a light by index.
         * @param index Light index to remove
         */
        void RemoveLight(std::uint32_t index);

        /**
         * @brief Updates a light's position.
         * @param index Light index to update
         * @param position New position
         */
        void UpdateLightPosition(std::uint32_t    index,
                                 const glm::vec3& position);

        /**
         * @brief Replaces all parameters of a light.
         * @param index Light index to update
         * @param light New light data
         */
        void UpdateLight(std::uint32_t index, const Light& light);

        /**
         * @brief Returns a light by index, or nullptr if out of range.
         */
        const Light* GetLight(std::uint32_t index) const;

        /**
         * @brief Clears all lights.
         */
        void ClearLights();

        /**
         * @brief Updates the GPU buffer with current light data.
         * @param frameIndex    Current frame index
         * @param viewPosition  Camera/eye position for attenuation calculations
         */
        void Update(std::uint32_t frameIndex, const glm::vec3& viewPosition);

        /**
         * @brief Returns the number of active lights.
         */
        std::uint32_t GetLightCount() const { return mLightCount; }

        /**
         * @brief Returns the light buffer.
         */
        skr::Arc<Buffer> GetBuffer() const { return mBuffer; }

        /**
         * @brief Returns the light descriptor set layout.
         */
        vk::DescriptorSetLayout GetLayout() const { return mLayout; }

        /**
         * @brief Returns the descriptor pool.
         */
        vk::DescriptorPool GetPool() const { return mPool; }

        /**
         * @brief Returns a descriptor set for a frame.
         * @param frameIndex Frame index
         * @return Descriptor set handle
         */
        vk::DescriptorSet GetSet(std::uint32_t frameIndex) const
        {
            return mSets[frameIndex];
        }

        /**
         * @brief Returns the maximum number of lights.
         */
        std::uint32_t GetMaxLights() const { return mMaxLights; }

        /**
         * @brief Sets IBL intensity written into the light UBO each Update.
         */
        void SetIblIntensity(float intensity) { mIblIntensity = intensity; }

        float GetIblIntensity() const { return mIblIntensity; }

        /**
         * @brief Sets exposure written into the light UBO each Update.
         */
        void SetExposure(float exposure) { mExposure = exposure; }

        float GetExposure() const { return mExposure; }

        /**
         * @brief Returns whether the service has any lights.
         */
        bool HasLights() const { return mLightCount > 0; }

      private:
        /**
         * @brief Creates the Vulkan descriptor resources.
         */
        void createDescriptorResources();

        /**
         * @brief Updates descriptor sets after light data changes.
         */
        void updateDescriptorSets();

        skr::Arc<Device> mDevice;
        std::uint32_t    mFrameCount;
        std::uint32_t    mMaxLights;
        std::uint32_t    mLightCount;
        float            mIblIntensity = 0.7f;
        float            mExposure     = 0.7f;

        std::vector<Light> mLights;
        skr::Arc<Buffer>   mBuffer;

        vk::DescriptorSetLayout        mLayout;
        vk::DescriptorPool             mPool;
        std::vector<vk::DescriptorSet> mSets;
    };

} // namespace FREYA_NAMESPACE