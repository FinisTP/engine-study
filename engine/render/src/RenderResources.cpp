#define VMA_IMPLEMENTATION
#define VMA_STATIC_VULKAN_FUNCTIONS 0
#define VMA_DYNAMIC_VULKAN_FUNCTIONS 1
#include "RenderResources.hpp"

#include <Log.hpp>
#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <tiny_gltf.h>

#include <algorithm>
#include <array>
#include <cstring>
#include <limits>
#include <string>
#include <type_traits>
#include <utility>

namespace
{
    void LogVkFailure(const char *operation, VkResult result)
    {
        umbra::diagnostics::Error(std::string{operation} + " VkResult=" + std::to_string(static_cast<int>(result)) + ".");
    }

    struct BufferCopyContext final
    {
        VkBuffer source;
        VkBuffer destination;
        VkDeviceSize size;
    };

    void RecordCopy(VkCommandBuffer commandBuffer, void *rawContext)
    {
        const auto &context = *static_cast<BufferCopyContext *>(rawContext);
        const VkBufferCopy region{0, 0, context.size};
        vkCmdCopyBuffer(commandBuffer, context.source, context.destination, 1, &region);
    }

    struct TextureUploadContext final
    {
        VkBuffer staging;
        VkImage image;
        std::uint32_t width;
        std::uint32_t height;
    };

    void RecordTextureUpload(VkCommandBuffer commandBuffer, void *rawContext)
    {
        const auto &context = *static_cast<TextureUploadContext *>(rawContext);
        // use TRANSFER_DST_OPTIMAL layout first to prepare transfer from staging
        VkImageMemoryBarrier toTransfer{};
        toTransfer.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        toTransfer.srcAccessMask = 0;
        toTransfer.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        toTransfer.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        toTransfer.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        toTransfer.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        toTransfer.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        toTransfer.image = context.image;
        toTransfer.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        toTransfer.subresourceRange.levelCount = 1;
        toTransfer.subresourceRange.layerCount = 1;
        vkCmdPipelineBarrier(
            commandBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
            0, 0, nullptr, 0, nullptr, 1, &toTransfer);

        VkBufferImageCopy copy{};
        copy.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        copy.imageSubresource.layerCount = 1;
        copy.imageExtent = {context.width, context.height, 1};
        vkCmdCopyBufferToImage(
            commandBuffer, context.staging, context.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy);

            // use SHADER_READ_ONLY_OPTIMAL layout for shader access
        VkImageMemoryBarrier toShader = toTransfer;
        toShader.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        toShader.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        toShader.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        toShader.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        vkCmdPipelineBarrier(
            commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            0, 0, nullptr, 0, nullptr, 1, &toShader);
    }

    [[nodiscard]] const unsigned char *AccessorData(const tinygltf::Model &model, const tinygltf::Accessor &accessor, std::size_t &stride)
    {
        if (accessor.bufferView < 0 || static_cast<std::size_t>(accessor.bufferView) >= model.bufferViews.size())
        {
            return nullptr;
        }
        const tinygltf::BufferView &view = model.bufferViews[static_cast<std::size_t>(accessor.bufferView)];
        if (view.buffer < 0 || static_cast<std::size_t>(view.buffer) >= model.buffers.size())
        {
            return nullptr;
        }
        stride = accessor.ByteStride(view);
        const tinygltf::Buffer &buffer = model.buffers[static_cast<std::size_t>(view.buffer)];
        const std::size_t offset = view.byteOffset + accessor.byteOffset;
        return offset < buffer.data.size() ? buffer.data.data() + offset : nullptr;
    }

    template <typename T>
    [[nodiscard]] T ReadScalar(const unsigned char *data, int componentType)
    {
        switch (componentType)
        {
        case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:
            return static_cast<T>(*data);
        case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
        {
            std::uint16_t value{};
            std::memcpy(&value, data, sizeof(value));
            return static_cast<T>(value);
        }
        case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:
        {
            std::uint32_t value{};
            std::memcpy(&value, data, sizeof(value));
            return static_cast<T>(value);
        }
        default:
            return {};
        }
    }
}

namespace umbra::render
{
    GpuBuffer::~GpuBuffer() { Reset(); }
    void GpuBuffer::Flush() const noexcept
    {
        if (allocator_ != VK_NULL_HANDLE && allocation_ != VK_NULL_HANDLE)
            (void)vmaFlushAllocation(allocator_, allocation_, 0, VK_WHOLE_SIZE);
    }
    GpuBuffer::GpuBuffer(GpuBuffer &&other) noexcept { *this = std::move(other); }
    GpuBuffer &GpuBuffer::operator=(GpuBuffer &&other) noexcept
    {
        if (this != &other)
        {
            Reset();
            allocator_ = std::exchange(other.allocator_, VK_NULL_HANDLE);
            buffer_ = std::exchange(other.buffer_, VK_NULL_HANDLE);
            allocation_ = std::exchange(other.allocation_, VK_NULL_HANDLE);
            mappedData_ = std::exchange(other.mappedData_, nullptr);
            size_ = std::exchange(other.size_, 0);
        }
        return *this;
    }
    void GpuBuffer::Reset() noexcept
    {
        if (allocator_ != VK_NULL_HANDLE && buffer_ != VK_NULL_HANDLE)
        {
            vmaDestroyBuffer(allocator_, buffer_, allocation_);
        }
        allocator_ = VK_NULL_HANDLE;
        buffer_ = VK_NULL_HANDLE;
        allocation_ = VK_NULL_HANDLE;
        mappedData_ = nullptr;
        size_ = 0;
    }

    GpuImage::~GpuImage() { Reset(); }
    GpuImage::GpuImage(GpuImage &&other) noexcept { *this = std::move(other); }
    GpuImage &GpuImage::operator=(GpuImage &&other) noexcept
    {
        if (this != &other)
        {
            Reset();
            allocator_ = std::exchange(other.allocator_, VK_NULL_HANDLE);
            device_ = std::exchange(other.device_, VK_NULL_HANDLE);
            image_ = std::exchange(other.image_, VK_NULL_HANDLE);
            view_ = std::exchange(other.view_, VK_NULL_HANDLE);
            allocation_ = std::exchange(other.allocation_, VK_NULL_HANDLE);
            format_ = std::exchange(other.format_, VK_FORMAT_UNDEFINED);
        }
        return *this;
    }
    void GpuImage::Reset() noexcept
    {
        if (device_ != VK_NULL_HANDLE && view_ != VK_NULL_HANDLE)
            vkDestroyImageView(device_, view_, nullptr);
        if (allocator_ != VK_NULL_HANDLE && image_ != VK_NULL_HANDLE)
            vmaDestroyImage(allocator_, image_, allocation_);
        allocator_ = VK_NULL_HANDLE;
        device_ = VK_NULL_HANDLE;
        image_ = VK_NULL_HANDLE;
        view_ = VK_NULL_HANDLE;
        allocation_ = VK_NULL_HANDLE;
        format_ = VK_FORMAT_UNDEFINED;
    }

    VulkanAllocator::~VulkanAllocator()
    {
        Shutdown();
    }

    bool VulkanAllocator::Initialize(
        VkInstance instance,
        VkPhysicalDevice physicalDevice,
        VkDevice device,
        VkQueue graphicsQueue,
        std::uint32_t graphicsQueueFamily)
    {
        // first, we create a VMA allocator
        VmaVulkanFunctions functions{};
        functions.vkGetInstanceProcAddr = vkGetInstanceProcAddr;
        functions.vkGetDeviceProcAddr = vkGetDeviceProcAddr;

        VmaAllocatorCreateInfo allocatorInfo{};
        allocatorInfo.instance = instance;
        allocatorInfo.physicalDevice = physicalDevice;
        allocatorInfo.device = device;
        allocatorInfo.vulkanApiVersion = VK_API_VERSION_1_3;
        allocatorInfo.pVulkanFunctions = &functions;
        if (vmaCreateAllocator(&allocatorInfo, &allocator_) != VK_SUCCESS)
        {
            diagnostics::Error("Failed to initialize Vulkan Memory Allocator.");
            return false;
        }

        device_ = device;
        graphicsQueue_ = graphicsQueue;

        // then we create a command pool for staging uploads
        VkCommandPoolCreateInfo poolInfo{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
        poolInfo.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
        poolInfo.queueFamilyIndex = graphicsQueueFamily;
        const VkResult result = vkCreateCommandPool(device_, &poolInfo, nullptr, &transferPool_);
        if (result != VK_SUCCESS)
        {
            LogVkFailure("Failed to create staging command pool.", result);
            Shutdown();
            return false;
        }
        diagnostics::Info("VMA GPU allocator initialized.");
        return true;
    }

    void VulkanAllocator::Shutdown() noexcept
    {
        if (device_ != VK_NULL_HANDLE && transferPool_ != VK_NULL_HANDLE)
            vkDestroyCommandPool(device_, transferPool_, nullptr);
        if (allocator_ != VK_NULL_HANDLE)
            vmaDestroyAllocator(allocator_);
        transferPool_ = VK_NULL_HANDLE;
        allocator_ = VK_NULL_HANDLE;
        graphicsQueue_ = VK_NULL_HANDLE;
        device_ = VK_NULL_HANDLE;
    }

    bool VulkanAllocator::CreateMappedBuffer(VkDeviceSize size, VkBufferUsageFlags usage, GpuBuffer &buffer)
    {
        buffer.Reset();

        VkBufferCreateInfo bufferInfo{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
        bufferInfo.size = size;
        bufferInfo.usage = usage;
        bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        VmaAllocationCreateInfo allocationInfo{};
        allocationInfo.usage = VMA_MEMORY_USAGE_AUTO;
        allocationInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;

        VmaAllocationInfo resultInfo{};
        const VkResult result = vmaCreateBuffer(allocator_, &bufferInfo, &allocationInfo, &buffer.buffer_, &buffer.allocation_, &resultInfo);
        if (result != VK_SUCCESS)
        {
            LogVkFailure("Failed to allocate mapped buffer.", result);
            return false;
        }
        buffer.allocator_ = allocator_;
        buffer.mappedData_ = resultInfo.pMappedData;
        buffer.size_ = size;
        return true;
    }

    bool VulkanAllocator::CreateDeviceBuffer(const void *data, VkDeviceSize size, VkBufferUsageFlags usage, GpuBuffer &buffer)
    {
        GpuBuffer staging;
        if (!CreateMappedBuffer(size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, staging))
        {
            return false;
        }
        std::memcpy(staging.GetMappedData(), data, static_cast<std::size_t>(size));
        staging.Flush();

        VkBufferCreateInfo bufferInfo{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
        bufferInfo.size = size;
        bufferInfo.usage = usage | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
        bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        VmaAllocationCreateInfo allocationInfo{};
        allocationInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
        const VkResult result = vmaCreateBuffer(allocator_, &bufferInfo, &allocationInfo, &buffer.buffer_, &buffer.allocation_, nullptr);
        if (result != VK_SUCCESS)
        {
            LogVkFailure("Failed to allocate device buffer.", result);
            return false;
        }
        buffer.allocator_ = allocator_;
        buffer.size_ = size;
        if (!CopyBuffer(staging.GetHandle(), buffer.GetHandle(), size))
        {
            buffer.Reset();
            return false;
        }
        return true;
    }

    bool VulkanAllocator::SubmitImmediate(void (*record)(VkCommandBuffer, void *), void *context)
    {
        VkCommandBufferAllocateInfo allocateInfo{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
        allocateInfo.commandPool = transferPool_;
        allocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocateInfo.commandBufferCount = 1;
        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        VkResult result = vkAllocateCommandBuffers(device_, &allocateInfo, &commandBuffer);
        if (result != VK_SUCCESS)
        {
            return false;
        }
        VkCommandBufferBeginInfo beginInfo{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        result = vkBeginCommandBuffer(commandBuffer, &beginInfo);
        if (result == VK_SUCCESS)
        {
            record(commandBuffer, context);
            result = vkEndCommandBuffer(commandBuffer);
        }
        if (result == VK_SUCCESS)
        {
            VkSubmitInfo submitInfo{VK_STRUCTURE_TYPE_SUBMIT_INFO};
            submitInfo.commandBufferCount = 1;
            submitInfo.pCommandBuffers = &commandBuffer;
            result = vkQueueSubmit(graphicsQueue_, 1, &submitInfo, VK_NULL_HANDLE);
        }
        if (result == VK_SUCCESS)
        {
            result = vkQueueWaitIdle(graphicsQueue_);
        }
        vkFreeCommandBuffers(device_, transferPool_, 1, &commandBuffer);
        if (result != VK_SUCCESS)
        {
            LogVkFailure("Failed staging upload.", result);
            return false;
        }
        return true;
    }

    bool VulkanAllocator::CopyBuffer(VkBuffer source, VkBuffer destination, VkDeviceSize size)
    {
        BufferCopyContext context{source, destination, size};
        return SubmitImmediate(RecordCopy, &context);
    }

    bool VulkanAllocator::CreateTexture(std::span<const std::byte> rgbaPixels, std::uint32_t width, std::uint32_t height, GpuImage &image)
    {
        const VkDeviceSize byteSize = static_cast<VkDeviceSize>(width) * height * 4;
        if (rgbaPixels.size_bytes() < byteSize || width == 0 || height == 0)
        {
            return false;
        }
        GpuBuffer staging;
        if (!CreateMappedBuffer(byteSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, staging))
        {
            return false;
        }
        std::memcpy(staging.GetMappedData(), rgbaPixels.data(), static_cast<std::size_t>(byteSize));
        staging.Flush();

        VkImageCreateInfo imageInfo{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
        imageInfo.imageType = VK_IMAGE_TYPE_2D;
        imageInfo.extent = {width, height, 1};
        imageInfo.mipLevels = 1;
        imageInfo.arrayLayers = 1;
        imageInfo.format = VK_FORMAT_R8G8B8A8_SRGB;
        imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        imageInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        VmaAllocationCreateInfo allocationInfo{};
        allocationInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
        VkResult result = vmaCreateImage(allocator_, &imageInfo, &allocationInfo, &image.image_, &image.allocation_, nullptr);
        if (result != VK_SUCCESS)
        {
            LogVkFailure("Failed to allocate texture image.", result);
            return false;
        }
        image.allocator_ = allocator_;
        image.device_ = device_;
        image.format_ = imageInfo.format;
        TextureUploadContext context{staging.GetHandle(), image.GetHandle(), width, height};
        if (!SubmitImmediate(RecordTextureUpload, &context))
        {
            image.Reset();
            return false;
        }

        VkImageViewCreateInfo viewInfo{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
        viewInfo.image = image.GetHandle();
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = imageInfo.format;
        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.layerCount = 1;
        result = vkCreateImageView(device_, &viewInfo, nullptr, &image.view_);
        if (result != VK_SUCCESS)
        {
            LogVkFailure("Failed to create texture view.", result);
            image.Reset();
            return false;
        }
        return true;
    }

    bool VulkanAllocator::CreateDepthImage(VkExtent2D extent, VkFormat format, GpuImage &image)
    {
        VkImageCreateInfo imageInfo{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
        imageInfo.imageType = VK_IMAGE_TYPE_2D;
        imageInfo.extent = {extent.width, extent.height, 1};
        imageInfo.mipLevels = 1;
        imageInfo.arrayLayers = 1;
        imageInfo.format = format;
        imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        imageInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
        imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        VmaAllocationCreateInfo allocationInfo{};
        allocationInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
        VkResult result = vmaCreateImage(allocator_, &imageInfo, &allocationInfo, &image.image_, &image.allocation_, nullptr);
        if (result != VK_SUCCESS)
        {
            LogVkFailure("Failed to allocate depth image.", result);
            return false;
        }
        image.allocator_ = allocator_;
        image.device_ = device_;
        image.format_ = format;
        VkImageViewCreateInfo viewInfo{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
        viewInfo.image = image.GetHandle();
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = format;
        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
        if (format == VK_FORMAT_D32_SFLOAT_S8_UINT || format == VK_FORMAT_D24_UNORM_S8_UINT)
        {
            viewInfo.subresourceRange.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
        }
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.layerCount = 1;
        result = vkCreateImageView(device_, &viewInfo, nullptr, &image.view_);
        if (result != VK_SUCCESS)
        {
            LogVkFailure("Failed to create depth view.", result);
            image.Reset();
            return false;
        }
        return true;
    }

    TextureResource::~TextureResource()
    {
        if (device != VK_NULL_HANDLE && sampler != VK_NULL_HANDLE)
        {
            vkDestroySampler(device, sampler, nullptr);
        }
    }
    TextureResource::TextureResource(TextureResource &&other) noexcept { *this = std::move(other); }
    TextureResource &TextureResource::operator=(TextureResource &&other) noexcept
    {
        if (this != &other)
        {
            if (device != VK_NULL_HANDLE && sampler != VK_NULL_HANDLE)
            {
                vkDestroySampler(device, sampler, nullptr);
            }
            image = std::move(other.image);
            sampler = std::exchange(other.sampler, VK_NULL_HANDLE);
            device = std::exchange(other.device, VK_NULL_HANDLE);
        }
        return *this;
    }

    SceneResources::~SceneResources() { Shutdown(); }
    const MeshResource *SceneResources::GetMesh(MeshHandle handle) const noexcept
    {
        return handle.value > 0 && handle.value <= meshes_.size() ? &meshes_[handle.value - 1] : nullptr;
    }
    const TextureResource *SceneResources::GetTexture(TextureHandle handle) const noexcept
    {
        return handle.value > 0 && handle.value <= textures_.size() ? &textures_[handle.value - 1] : nullptr;
    }

    bool SceneResources::LoadGltf(
        const std::filesystem::path &path, VulkanAllocator &allocator, VkDevice device,
        const VkPhysicalDeviceProperties &deviceProperties)
    {
        tinygltf::TinyGLTF loader;
        tinygltf::Model model;
        std::string warning;
        std::string error;
        const bool loaded = path.extension() == ".glb"
                                ? loader.LoadBinaryFromFile(&model, &error, &warning, path.string())
                                : loader.LoadASCIIFromFile(&model, &error, &warning, path.string());
        if (!warning.empty())
        {
            diagnostics::Info("glTF: " + warning);
        }
        if (!loaded)
        {
            diagnostics::Error("Failed loading glTF scene: " + error);
            return false;
        }
        if (model.meshes.empty() || model.meshes.front().primitives.empty())
        {
            diagnostics::Error("glTF scene has no mesh primitive.");
            return false;
        }
        const tinygltf::Primitive &primitive = model.meshes.front().primitives.front();
        const auto positionIt = primitive.attributes.find("POSITION");
        if (positionIt == primitive.attributes.end() || primitive.indices < 0)
        {
            diagnostics::Error("glTF primitive requires POSITION and indices.");
            return false;
        }
        const tinygltf::Accessor &positions = model.accessors[static_cast<std::size_t>(positionIt->second)];
        if (positions.componentType != TINYGLTF_COMPONENT_TYPE_FLOAT || positions.type != TINYGLTF_TYPE_VEC3)
        {
            diagnostics::Error("glTF POSITION must be float VEC3.");
            return false;
        }
        std::size_t positionStride{};
        const unsigned char *positionData = AccessorData(model, positions, positionStride);
        if (positionData == nullptr)
            return false;
        std::vector<Vertex> vertices(positions.count);
        for (std::size_t index = 0; index < vertices.size(); ++index)
            std::memcpy(&vertices[index].position, positionData + index * positionStride, sizeof(glm::vec3));

        const auto readFloatAttribute = [&](const char *name, int expectedType, auto member)
        {
            const auto it = primitive.attributes.find(name);
            if (it == primitive.attributes.end())
                return;
            const tinygltf::Accessor &accessor = model.accessors[static_cast<std::size_t>(it->second)];
            if (accessor.componentType != TINYGLTF_COMPONENT_TYPE_FLOAT || accessor.type != expectedType ||
                accessor.count != vertices.size())
                return;
            std::size_t stride{};
            const unsigned char *data = AccessorData(model, accessor, stride);
            if (data == nullptr)
                return;
            using Value = std::remove_reference_t<decltype(vertices[0].*member)>;
            for (std::size_t index = 0; index < vertices.size(); ++index)
                std::memcpy(&(vertices[index].*member), data + index * stride, sizeof(Value));
        };
        readFloatAttribute("NORMAL", TINYGLTF_TYPE_VEC3, &Vertex::normal);
        readFloatAttribute("TEXCOORD_0", TINYGLTF_TYPE_VEC2, &Vertex::uv);

        const tinygltf::Accessor &indices = model.accessors[static_cast<std::size_t>(primitive.indices)];
        std::size_t indexStride{};
        const unsigned char *indexData = AccessorData(model, indices, indexStride);
        if (indexData == nullptr)
            return false;
        std::vector<std::uint32_t> indexValues(indices.count);
        for (std::size_t index = 0; index < indexValues.size(); ++index)
            indexValues[index] = ReadScalar<std::uint32_t>(indexData + index * indexStride, indices.componentType);

        MeshResource mesh;
        if (!allocator.CreateDeviceBuffer(vertices.data(), vertices.size() * sizeof(Vertex),
                                          VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, mesh.vertexBuffer) ||
            !allocator.CreateDeviceBuffer(indexValues.data(), indexValues.size() * sizeof(std::uint32_t),
                                          VK_BUFFER_USAGE_INDEX_BUFFER_BIT, mesh.indexBuffer))
            return false;
        mesh.indexCount = static_cast<std::uint32_t>(indexValues.size());
        if (primitive.material >= 0)
        {
            const auto &factor = model.materials[static_cast<std::size_t>(primitive.material)].pbrMetallicRoughness.baseColorFactor;
            if (factor.size() == 4)
                mesh.baseColor = glm::vec4(factor[0], factor[1], factor[2], factor[3]);
        }

        const tinygltf::Image *sourceImage = model.images.empty() ? nullptr : &model.images.front();
        if (primitive.material >= 0)
        {
            const int textureIndex = model.materials[static_cast<std::size_t>(primitive.material)]
                                         .pbrMetallicRoughness.baseColorTexture.index;
            if (textureIndex >= 0 && static_cast<std::size_t>(textureIndex) < model.textures.size())
            {
                const int source = model.textures[static_cast<std::size_t>(textureIndex)].source;
                if (source >= 0 && static_cast<std::size_t>(source) < model.images.size())
                    sourceImage = &model.images[static_cast<std::size_t>(source)];
            }
        }
        if (sourceImage == nullptr || sourceImage->width <= 0 || sourceImage->height <= 0)
        {
            diagnostics::Error("glTF scene requires one decoded texture.");
            return false;
        }
        std::vector<unsigned char> rgba(static_cast<std::size_t>(sourceImage->width * sourceImage->height * 4));
        for (int pixel = 0; pixel < sourceImage->width * sourceImage->height; ++pixel)
        {
            for (int channel = 0; channel < 4; ++channel)
            {
                rgba[static_cast<std::size_t>(pixel * 4 + channel)] = channel < sourceImage->component
                                                                          ? sourceImage->image[static_cast<std::size_t>(pixel * sourceImage->component + channel)]
                                                                          : 255;
            }
        }
        TextureResource texture;
        if (!allocator.CreateTexture(
                std::as_bytes(std::span{rgba}), static_cast<std::uint32_t>(sourceImage->width),
                static_cast<std::uint32_t>(sourceImage->height), texture.image))
            return false;
        texture.device = device;
        VkSamplerCreateInfo samplerInfo{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
        samplerInfo.magFilter = VK_FILTER_LINEAR;
        samplerInfo.minFilter = VK_FILTER_LINEAR;
        samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerInfo.anisotropyEnable = VK_TRUE;
        samplerInfo.maxAnisotropy = std::min(8.0F, deviceProperties.limits.maxSamplerAnisotropy);
        samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
        samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        const VkResult samplerResult = vkCreateSampler(device, &samplerInfo, nullptr, &texture.sampler);
        if (samplerResult != VK_SUCCESS)
        {
            LogVkFailure("Failed creating texture sampler.", samplerResult);
            return false;
        }

        meshes_.push_back(std::move(mesh));
        textures_.push_back(std::move(texture));
        sceneMesh_.value = static_cast<std::uint32_t>(meshes_.size());
        sceneTexture_.value = static_cast<std::uint32_t>(textures_.size());
        diagnostics::Info("Loaded glTF primitive: " + std::to_string(vertices.size()) + " vertices, " +
                          std::to_string(indexValues.size()) + " indices, one texture.");
        return true;
    }

    void SceneResources::Shutdown() noexcept
    {
        textures_.clear();
        meshes_.clear();
        sceneMesh_ = {};
        sceneTexture_ = {};
    }
}
