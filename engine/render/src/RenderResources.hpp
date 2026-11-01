#pragma once

#include <volk.h>
#include <vk_mem_alloc.h>

#include <DrawList.hpp>
#include <MeshData.hpp>
#include <RetireQueue.hpp>
#include <SlotMap.hpp>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <span>
#include <variant>
#include <vector>

namespace umbra::render
{
    // Vertex layout shared with the importer; see core MeshData.hpp.
    using core::Vertex;

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
        // set 1 of the scene pipeline, allocated from (and freed back to) pool
        VkDescriptorSet descriptorSet = VK_NULL_HANDLE;
        VkDescriptorPool pool = VK_NULL_HANDLE;

        TextureResource() = default;
        ~TextureResource();
        TextureResource(const TextureResource &) = delete;
        TextureResource &operator=(const TextureResource &) = delete;
        TextureResource(TextureResource &&other) noexcept;
        TextureResource &operator=(TextureResource &&other) noexcept;
        void Release() noexcept;
    };

    // Owns every mesh and texture. Handles are generational (see core SlotMap.md);
    // destroyed or replaced GPU data is kept alive until the GPU is done with it
    // (see ResourceLifetime.md).
    class SceneResources final
    {
    public:
        static constexpr std::uint64_t framesInFlight = 2;

        SceneResources() = default;
        ~SceneResources();
        SceneResources(const SceneResources &) = delete;
        SceneResources &operator=(const SceneResources &) = delete;

        // Creates the texture descriptor layout/pool and the 1x1 white fallback texture.
        [[nodiscard]] bool Initialize(VkDevice device, VulkanAllocator &allocator, const VkPhysicalDeviceProperties &deviceProperties);
        void Shutdown() noexcept;

        // Return an invalid handle (and log why) if the data is invalid or upload fails.
        [[nodiscard]] MeshHandle CreateMesh(const core::MeshData &data);
        [[nodiscard]] TextureHandle CreateTexture(const core::TextureData &data);
        // Swap new data in behind an existing handle; the old GPU data is retired.
        // On failure the old data stays in place.
        bool ReplaceMesh(MeshHandle handle, const core::MeshData &data);
        bool ReplaceTexture(TextureHandle handle, const core::TextureData &data);
        // The handle goes stale immediately; GPU memory is freed once no frame uses it.
        bool DestroyMesh(MeshHandle handle);
        bool DestroyTexture(TextureHandle handle);

        // Call once after each submitted frame; frees retired resources the GPU has finished with.
        void OnFrameSubmitted();
        // Frees all retired resources. Only call while the device is idle.
        void FlushRetired() { retired_.Flush(); }

        [[nodiscard]] const MeshResource *GetMesh(MeshHandle handle) const noexcept { return meshes_.Get(handle); }
        [[nodiscard]] const TextureResource *GetTexture(TextureHandle handle) const noexcept { return textures_.Get(handle); }
        [[nodiscard]] TextureHandle GetFallbackTexture() const noexcept { return fallbackTexture_; }
        [[nodiscard]] std::size_t GetMeshCount() const noexcept { return meshes_.Size(); }
        [[nodiscard]] std::size_t GetTextureCount() const noexcept { return textures_.Size(); }
        [[nodiscard]] std::size_t GetRetiredCount() const noexcept { return retired_.Size(); }
        [[nodiscard]] VkDescriptorSetLayout GetTextureSetLayout() const noexcept { return textureSetLayout_; }

    private:
        [[nodiscard]] bool UploadMesh(const core::MeshData &data, MeshResource &mesh);
        [[nodiscard]] bool UploadTexture(const core::TextureData &data, TextureResource &texture);

        VkDevice device_ = VK_NULL_HANDLE;
        VulkanAllocator *allocator_ = nullptr;
        float maxAnisotropy_ = 1.0F;
        VkDescriptorSetLayout textureSetLayout_ = VK_NULL_HANDLE;
        VkDescriptorPool texturePool_ = VK_NULL_HANDLE;
        core::SlotMap<MeshResource, core::MeshTag> meshes_;
        core::SlotMap<TextureResource, core::TextureTag> textures_;
        core::RetireQueue<std::variant<MeshResource, TextureResource>> retired_;
        std::uint64_t frameSerial_ = 0;
        TextureHandle fallbackTexture_{};
    };

    // Reads the first primitive and its base-color texture from a glTF file.
    // Temporary: replaced by the Assets module in P3.5.
    [[nodiscard]] bool ReadGltfPrimitive(const std::filesystem::path &path, core::MeshData &mesh, core::TextureData &texture);
}
