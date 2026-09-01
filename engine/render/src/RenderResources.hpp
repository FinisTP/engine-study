#pragma once

#include <volk.h>
#include <vk_mem_alloc.h>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <span>
#include <vector>

namespace umbra::render
{
    struct Vertex final
    {
        glm::vec3 position{};
        glm::vec3 normal{};
        glm::vec2 uv{};
    };

    class GpuBuffer final
    {
    public:
        GpuBuffer() = default;
        ~GpuBuffer();
        GpuBuffer(const GpuBuffer &) = delete;
        GpuBuffer &operator=(const GpuBuffer &) = delete;
        GpuBuffer(GpuBuffer &&other) noexcept;
        GpuBuffer &operator=(GpuBuffer &&other) noexcept;

        void Reset() noexcept;
        void Flush() const noexcept;
        [[nodiscard]] VkBuffer GetHandle() const noexcept { return buffer_; }
        [[nodiscard]] void *GetMappedData() const noexcept { return mappedData_; }
        [[nodiscard]] VkDeviceSize GetSize() const noexcept { return size_; }

    private:
        friend class VulkanAllocator;
        VmaAllocator allocator_ = VK_NULL_HANDLE;
        VkBuffer buffer_ = VK_NULL_HANDLE;
        VmaAllocation allocation_ = VK_NULL_HANDLE;
        void *mappedData_ = nullptr;
        VkDeviceSize size_ = 0;
    };

    class GpuImage final
    {
    public:
        GpuImage() = default;
        ~GpuImage();
        GpuImage(const GpuImage &) = delete;
        GpuImage &operator=(const GpuImage &) = delete;
        GpuImage(GpuImage &&other) noexcept;
        GpuImage &operator=(GpuImage &&other) noexcept;

        void Reset() noexcept;
        [[nodiscard]] VkImage GetHandle() const noexcept { return image_; }
        [[nodiscard]] VkImageView GetView() const noexcept { return view_; }
        [[nodiscard]] VkFormat GetFormat() const noexcept { return format_; }

    private:
        friend class VulkanAllocator;
        VmaAllocator allocator_ = VK_NULL_HANDLE;
        VkDevice device_ = VK_NULL_HANDLE;
        VkImage image_ = VK_NULL_HANDLE;
        VkImageView view_ = VK_NULL_HANDLE;
        VmaAllocation allocation_ = VK_NULL_HANDLE;
        VkFormat format_ = VK_FORMAT_UNDEFINED;
    };

    class VulkanAllocator final
    {
    public:
        VulkanAllocator() = default;
        ~VulkanAllocator();
        VulkanAllocator(const VulkanAllocator &) = delete;
        VulkanAllocator &operator=(const VulkanAllocator &) = delete;

        [[nodiscard]] bool Initialize(
            VkInstance instance,
            VkPhysicalDevice physicalDevice,
            VkDevice device,
            VkQueue graphicsQueue,
            std::uint32_t graphicsQueueFamily);
        void Shutdown() noexcept;

        [[nodiscard]] bool CreateDeviceBuffer(
            const void *data,
            VkDeviceSize size,
            VkBufferUsageFlags usage,
            GpuBuffer &buffer);
        [[nodiscard]] bool CreateMappedBuffer(
            VkDeviceSize size,
            VkBufferUsageFlags usage,
            GpuBuffer &buffer);
        [[nodiscard]] bool CreateTexture(
            std::span<const std::byte> rgbaPixels,
            std::uint32_t width,
            std::uint32_t height,
            GpuImage &image);
        [[nodiscard]] bool CreateDepthImage(
            VkExtent2D extent,
            VkFormat format,
            GpuImage &image);

        [[nodiscard]] VmaAllocator GetHandle() const noexcept { return allocator_; }

    private:
        [[nodiscard]] bool CopyBuffer(VkBuffer source, VkBuffer destination, VkDeviceSize size);
        [[nodiscard]] bool SubmitImmediate(void (*record)(VkCommandBuffer, void *), void *context);

        VkDevice device_ = VK_NULL_HANDLE;
        VkQueue graphicsQueue_ = VK_NULL_HANDLE;
        VkCommandPool transferPool_ = VK_NULL_HANDLE;
        VmaAllocator allocator_ = VK_NULL_HANDLE;
    };

    struct MeshHandle final
    {
        std::uint32_t value = 0;
        [[nodiscard]] explicit operator bool() const noexcept { return value != 0; }
    };

    struct TextureHandle final
    {
        std::uint32_t value = 0;
        [[nodiscard]] explicit operator bool() const noexcept { return value != 0; }
    };

    struct MeshResource final
    {
        GpuBuffer vertexBuffer;
        GpuBuffer indexBuffer;
        std::uint32_t indexCount = 0;
        glm::vec4 baseColor{1.0F};
    };

    struct TextureResource final
    {
        GpuImage image;
        VkSampler sampler = VK_NULL_HANDLE;
        VkDevice device = VK_NULL_HANDLE;

        TextureResource() = default;
        ~TextureResource();
        TextureResource(const TextureResource &) = delete;
        TextureResource &operator=(const TextureResource &) = delete;
        TextureResource(TextureResource &&other) noexcept;
        TextureResource &operator=(TextureResource &&other) noexcept;
    };

    class SceneResources final
    {
    public:
        SceneResources() = default;
        ~SceneResources();
        SceneResources(const SceneResources &) = delete;
        SceneResources &operator=(const SceneResources &) = delete;

        [[nodiscard]] bool LoadGltf(
            const std::filesystem::path &path,
            VulkanAllocator &allocator,
            VkDevice device,
            const VkPhysicalDeviceProperties &deviceProperties);
        void Shutdown() noexcept;

        [[nodiscard]] const MeshResource *GetMesh(MeshHandle handle) const noexcept;
        [[nodiscard]] const TextureResource *GetTexture(TextureHandle handle) const noexcept;
        [[nodiscard]] MeshHandle GetSceneMesh() const noexcept { return sceneMesh_; }
        [[nodiscard]] TextureHandle GetSceneTexture() const noexcept { return sceneTexture_; }
        [[nodiscard]] std::size_t GetMeshCount() const noexcept { return meshes_.size(); }
        [[nodiscard]] std::size_t GetTextureCount() const noexcept { return textures_.size(); }

    private:
        std::vector<MeshResource> meshes_;
        std::vector<TextureResource> textures_;
        MeshHandle sceneMesh_{};
        TextureHandle sceneTexture_{};
    };
}
