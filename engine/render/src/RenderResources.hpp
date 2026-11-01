#pragma once

#include <volk.h>
#include <vk_mem_alloc.h>

#include <DrawList.hpp>
#include <SlotMap.hpp>

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
    // Represents a single vertex with position, normal, and texture coordinates.
    struct Vertex final
    {
        glm::vec3 position{};
        glm::vec3 normal{};
        glm::vec2 uv{};
    };

    // Wraps VkBuffer, think of it as GPU memory array
    class GpuBuffer final
    {
    public:
        GpuBuffer() = default;
        ~GpuBuffer();
        // copy is not allowed because two objects cannot own the same GPU memory, move is allowed to transfer ownership
        GpuBuffer(const GpuBuffer &) = delete;
        GpuBuffer &operator=(const GpuBuffer &) = delete;
        GpuBuffer(GpuBuffer &&other) noexcept;
        GpuBuffer &operator=(GpuBuffer &&other) noexcept;

        // Resets the buffer, releasing its resources.
        void Reset() noexcept;

        // Flushes any pending writes to the GPU. Important for uniform buffer
        void Flush() const noexcept;
        [[nodiscard]] VkBuffer GetHandle() const noexcept { return buffer_; }
        [[nodiscard]] void *GetMappedData() const noexcept { return mappedData_; }
        [[nodiscard]] VkDeviceSize GetSize() const noexcept { return size_; }

    private:
        friend class VulkanAllocator;
        VmaAllocator allocator_ = VK_NULL_HANDLE; // save to destroy the buffer later
        VkBuffer buffer_ = VK_NULL_HANDLE; // a handle to the GPU buffer object
        VmaAllocation allocation_ = VK_NULL_HANDLE; // save to destroy the buffer later
        void *mappedData_ = nullptr; // pointer to CPU-accessible memory mapped to the GPU buffer
        VkDeviceSize size_ = 0;
    };

    // Wraps VkImage and VkImageView, think of it as GPU texture
    // VkImage is raw pixel data, VkImageView describes how to access that data (like 2D color texture or depth texture)
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

    // Allocator built based on VMA (Vulkan Memory Allocator) to manage GPU memory for buffers and images.
    // Without VMA, we need to manually create buffer/allocate mem/bind mem for each buffer/image, which is cumbersome and error-prone.
    class VulkanAllocator final
    {
    public:
        VulkanAllocator() = default;
        ~VulkanAllocator();
        VulkanAllocator(const VulkanAllocator &) = delete;
        VulkanAllocator &operator=(const VulkanAllocator &) = delete;

        // Create a VMA allocator for the given Vulkan instance, physical device, and logical device.
        [[nodiscard]] bool Initialize(VkInstance instance, VkPhysicalDevice physicalDevice, VkDevice device, VkQueue graphicsQueue, std::uint32_t graphicsQueueFamily);
        void Shutdown() noexcept;

        // Create GPU-only memory buffer
        [[nodiscard]] bool CreateDeviceBuffer(const void *data, VkDeviceSize size, VkBufferUsageFlags usage, GpuBuffer &buffer);

        // Create CPU-writable buffer that is mapped using CPU address space.
        [[nodiscard]] bool CreateMappedBuffer(VkDeviceSize size, VkBufferUsageFlags usage, GpuBuffer &buffer);

        // Create a GPU-only texture from RGBA pixel data, and create a VkImageView for it.
        [[nodiscard]] bool CreateTexture(std::span<const std::byte> rgbaPixels, std::uint32_t width, std::uint32_t height, GpuImage &image);

        // Create a GPU-only depth image with the specified extent and format, and create a VkImageView for it.
        [[nodiscard]] bool CreateDepthImage(VkExtent2D extent, VkFormat format, GpuImage &image);

        [[nodiscard]] VmaAllocator GetHandle() const noexcept { return allocator_; }

    private:
        [[nodiscard]] bool CopyBuffer(VkBuffer source, VkBuffer destination, VkDeviceSize size);
        [[nodiscard]] bool SubmitImmediate(void (*record)(VkCommandBuffer, void *), void *context);

        VkDevice device_ = VK_NULL_HANDLE;
        VkQueue graphicsQueue_ = VK_NULL_HANDLE;
        VkCommandPool transferPool_ = VK_NULL_HANDLE;
        VmaAllocator allocator_ = VK_NULL_HANDLE;
    };

    using core::MeshHandle;
    using core::TextureHandle;

    struct MeshResource final
    {
        // holds the vertices (like dots)
        GpuBuffer vertexBuffer;
        // holds the indices (like lines connecting the dots), should be divisible by 3 for triangles
        // clockwise winding order is used for front face, counter-clockwise for back face
        GpuBuffer indexBuffer;
        std::uint32_t indexCount = 0;
        glm::vec4 baseColor{1.0F};
    };

    struct TextureResource final
    {
        GpuImage image;
        VkSampler sampler = VK_NULL_HANDLE;
        VkDevice device = VK_NULL_HANDLE;
        // set 1 of the scene pipeline; owned by SceneResources' descriptor pool
        VkDescriptorSet descriptorSet = VK_NULL_HANDLE;

        TextureResource() = default;
        ~TextureResource();
        TextureResource(const TextureResource &) = delete;
        TextureResource &operator=(const TextureResource &) = delete;
        TextureResource(TextureResource &&other) noexcept;
        TextureResource &operator=(TextureResource &&other) noexcept;
    };

    // Owns every mesh and texture. Handles are generational (see core SlotMap.md).
    class SceneResources final
    {
    public:
        SceneResources() = default;
        ~SceneResources();
        SceneResources(const SceneResources &) = delete;
        SceneResources &operator=(const SceneResources &) = delete;

        // Creates the per-texture descriptor set layout and pool.
        [[nodiscard]] bool Initialize(VkDevice device);
        // Load one mesh primitive and one texture from a glTF file
        [[nodiscard]] bool LoadGltf(const std::filesystem::path &path, VulkanAllocator &allocator, const VkPhysicalDeviceProperties &deviceProperties);
        void Shutdown() noexcept;

        [[nodiscard]] const MeshResource *GetMesh(MeshHandle handle) const noexcept { return meshes_.Get(handle); }
        [[nodiscard]] const TextureResource *GetTexture(TextureHandle handle) const noexcept { return textures_.Get(handle); }
        [[nodiscard]] MeshHandle GetSceneMesh() const noexcept { return sceneMesh_; }
        [[nodiscard]] TextureHandle GetSceneTexture() const noexcept { return sceneTexture_; }
        [[nodiscard]] std::size_t GetMeshCount() const noexcept { return meshes_.Size(); }
        [[nodiscard]] std::size_t GetTextureCount() const noexcept { return textures_.Size(); }
        [[nodiscard]] VkDescriptorSetLayout GetTextureSetLayout() const noexcept { return textureSetLayout_; }

    private:
        [[nodiscard]] bool WriteTextureSet(TextureResource &texture);

        VkDevice device_ = VK_NULL_HANDLE;
        VkDescriptorSetLayout textureSetLayout_ = VK_NULL_HANDLE;
        VkDescriptorPool texturePool_ = VK_NULL_HANDLE;
        core::SlotMap<MeshResource, core::MeshTag> meshes_;
        core::SlotMap<TextureResource, core::TextureTag> textures_;
        MeshHandle sceneMesh_{};
        TextureHandle sceneTexture_{};
    };
}
