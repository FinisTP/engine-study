#define VMA_IMPLEMENTATION
#define VMA_STATIC_VULKAN_FUNCTIONS 0
#define VMA_DYNAMIC_VULKAN_FUNCTIONS 1
#include "RenderResources.hpp"

#include <Log.hpp>

#include <algorithm>
#include <array>
#include <cstring>
#include <fstream>
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
        std::vector<VkBufferImageCopy> copies; // one per mip level
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
        toTransfer.subresourceRange.levelCount = static_cast<std::uint32_t>(context.copies.size());
        toTransfer.subresourceRange.layerCount = 1;
        vkCmdPipelineBarrier(
            commandBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
            0, 0, nullptr, 0, nullptr, 1, &toTransfer);

        vkCmdCopyBufferToImage(commandBuffer, context.staging, context.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                               static_cast<std::uint32_t>(context.copies.size()), context.copies.data());

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

}

namespace umbra::render
{
    bool LoadShaderModule(VkDevice device, const std::filesystem::path &path, VkShaderModule &shader)
    {
        // HLSL -> shader compiler -> SPIR-V file -> VkShaderModule
        std::ifstream file(path, std::ios::ate | std::ios::binary);
        if (!file)
        {
            umbra::diagnostics::Error("Failed opening shader: " + path.string());
            return false;
        }
        const auto size = file.tellg();
        if (size <= 0 || size % 4 != 0)
        {
            umbra::diagnostics::Error("Shader size is invalid: " + path.string());
            return false;
        }
        std::vector<std::uint32_t> code(static_cast<std::size_t>(size) / 4);
        file.seekg(0);
        file.read(reinterpret_cast<char *>(code.data()), size);
        VkShaderModuleCreateInfo info{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
        info.codeSize = code.size() * 4;
        info.pCode = code.data();
        return vkCreateShaderModule(device, &info, nullptr, &shader) == VK_SUCCESS;
    }


    GpuBuffer::~GpuBuffer() { Reset(); }
    void GpuBuffer::Flush() const noexcept
    {
        if (allocator_ != VK_NULL_HANDLE && allocation_ != VK_NULL_HANDLE)
            (void)vmaFlushAllocation(allocator_, allocation_, 0, VK_WHOLE_SIZE);
    }
    void GpuBuffer::Invalidate() const noexcept
    {
        if (allocator_ != VK_NULL_HANDLE && allocation_ != VK_NULL_HANDLE)
            (void)vmaInvalidateAllocation(allocator_, allocation_, 0, VK_WHOLE_SIZE);
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
        allocatorInfo.vulkanApiVersion = VK_API_VERSION_1_1; // the oldest version the renderer supports (phones)
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

    bool VulkanAllocator::CreateReadbackBuffer(VkDeviceSize size, GpuBuffer &buffer)
    {
        buffer.Reset();
        VkBufferCreateInfo bufferInfo{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
        bufferInfo.size = size;
        bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
        bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        VmaAllocationCreateInfo allocationInfo{};
        allocationInfo.usage = VMA_MEMORY_USAGE_AUTO;
        allocationInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;
        VmaAllocationInfo resultInfo{};
        const VkResult result = vmaCreateBuffer(allocator_, &bufferInfo, &allocationInfo, &buffer.buffer_, &buffer.allocation_, &resultInfo);
        if (result != VK_SUCCESS)
        {
            LogVkFailure("Failed to allocate read-back buffer.", result);
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

    bool VulkanAllocator::CreateTexture(const std::vector<core::TextureData> &levels, GpuImage &image)
    {
        // 1. All mip levels one after another in one staging buffer, one copy region each.
        if (levels.empty() || levels[0].width == 0 || levels[0].height == 0)
        {
            return false;
        }
        const std::uint32_t width = levels[0].width;
        const std::uint32_t height = levels[0].height;
        TextureUploadContext context{};
        VkDeviceSize byteSize = 0;
        for (std::uint32_t level = 0; level < levels.size(); ++level)
        {
            VkBufferImageCopy copy{};
            copy.bufferOffset = byteSize;
            copy.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            copy.imageSubresource.mipLevel = level;
            copy.imageSubresource.layerCount = 1;
            copy.imageExtent = {levels[level].width, levels[level].height, 1};
            context.copies.push_back(copy);
            byteSize += levels[level].rgba.size();
        }
        GpuBuffer staging;
        if (!CreateMappedBuffer(byteSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, staging))
        {
            return false;
        }
        for (std::size_t level = 0; level < levels.size(); ++level)
            std::memcpy(static_cast<std::byte *>(staging.GetMappedData()) + context.copies[level].bufferOffset, levels[level].rgba.data(),
                        levels[level].rgba.size());
        staging.Flush();

        VkImageCreateInfo imageInfo{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
        imageInfo.imageType = VK_IMAGE_TYPE_2D;
        imageInfo.extent = {width, height, 1};
        imageInfo.mipLevels = static_cast<std::uint32_t>(levels.size());
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
        context.staging = staging.GetHandle();
        context.image = image.GetHandle();
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
        viewInfo.subresourceRange.levelCount = imageInfo.mipLevels;
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

    bool VulkanAllocator::CreateColorTarget(VkExtent2D extent, VkFormat format, GpuImage &image)
    {
        VkImageCreateInfo imageInfo{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
        imageInfo.imageType = VK_IMAGE_TYPE_2D;
        imageInfo.extent = {std::max(extent.width, 1u), std::max(extent.height, 1u), 1};
        imageInfo.mipLevels = 1;
        imageInfo.arrayLayers = 1;
        imageInfo.format = format;
        imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        imageInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        VmaAllocationCreateInfo allocationInfo{};
        allocationInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
        VkResult result = vmaCreateImage(allocator_, &imageInfo, &allocationInfo, &image.image_, &image.allocation_, nullptr);
        if (result != VK_SUCCESS)
        {
            LogVkFailure("Failed to allocate a color target.", result);
            return false;
        }
        image.allocator_ = allocator_;
        image.device_ = device_;
        image.format_ = format;
        VkImageViewCreateInfo viewInfo{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
        viewInfo.image = image.GetHandle();
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = format;
        viewInfo.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
        result = vkCreateImageView(device_, &viewInfo, nullptr, &image.view_);
        if (result != VK_SUCCESS)
        {
            LogVkFailure("Failed to create a color target view.", result);
            image.Reset();
            return false;
        }
        return true;
    }

    bool VulkanAllocator::CreateLut3D(const core::TextureData &strip, GpuImage &image)
    {
        // 1. Strip (x = b * 16 + r, y = g) -> 3D order (r fastest, then g, then b).
        constexpr std::uint32_t n = core::lutSize;
        if (strip.width != n * n || strip.height != n)
            return false;
        GpuBuffer staging;
        if (!CreateMappedBuffer(n * n * n * 4, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, staging))
            return false;
        auto *out = static_cast<std::uint8_t *>(staging.GetMappedData());
        for (std::uint32_t b = 0; b < n; ++b)
            for (std::uint32_t g = 0; g < n; ++g)
                std::memcpy(out + ((b * n + g) * n) * 4, strip.rgba.data() + (g * n * n + b * n) * 4, n * 4);
        staging.Flush();
        // 2. The image (UNORM: the shader decodes sRGB itself) and its upload.
        VkImageCreateInfo imageInfo{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
        imageInfo.imageType = VK_IMAGE_TYPE_3D;
        imageInfo.extent = {n, n, n};
        imageInfo.mipLevels = 1;
        imageInfo.arrayLayers = 1;
        imageInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
        imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        imageInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        VmaAllocationCreateInfo allocationInfo{};
        allocationInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
        VkResult result = vmaCreateImage(allocator_, &imageInfo, &allocationInfo, &image.image_, &image.allocation_, nullptr);
        if (result != VK_SUCCESS)
        {
            LogVkFailure("Failed to allocate the color table.", result);
            return false;
        }
        image.allocator_ = allocator_;
        image.device_ = device_;
        image.format_ = imageInfo.format;
        TextureUploadContext context{};
        VkBufferImageCopy copy{};
        copy.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
        copy.imageExtent = {n, n, n};
        context.copies.push_back(copy);
        context.staging = staging.GetHandle();
        context.image = image.GetHandle();
        if (!SubmitImmediate(RecordTextureUpload, &context))
        {
            image.Reset();
            return false;
        }
        VkImageViewCreateInfo viewInfo{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
        viewInfo.image = image.GetHandle();
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_3D;
        viewInfo.format = imageInfo.format;
        viewInfo.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
        result = vkCreateImageView(device_, &viewInfo, nullptr, &image.view_);
        if (result != VK_SUCCESS)
        {
            image.Reset();
            return false;
        }
        return true;
    }

    bool VulkanAllocator::CreateDepthImage(VkExtent2D extent, VkFormat format, GpuImage &image, VkImageUsageFlags extraUsage)
    {
        VkImageCreateInfo imageInfo{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
        imageInfo.imageType = VK_IMAGE_TYPE_2D;
        imageInfo.extent = {extent.width, extent.height, 1};
        imageInfo.mipLevels = 1;
        imageInfo.arrayLayers = 1;
        imageInfo.format = format;
        imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        imageInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | extraUsage;
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

    TextureResource::~TextureResource() { Release(); }

    void TextureResource::Release() noexcept
    {
        if (device != VK_NULL_HANDLE && pool != VK_NULL_HANDLE && descriptorSet != VK_NULL_HANDLE)
        {
            vkFreeDescriptorSets(device, pool, 1, &descriptorSet);
        }
        if (device != VK_NULL_HANDLE && sampler != VK_NULL_HANDLE)
        {
            vkDestroySampler(device, sampler, nullptr);
        }
        descriptorSet = VK_NULL_HANDLE;
        sampler = VK_NULL_HANDLE;
        image.Reset();
    }
    TextureResource::TextureResource(TextureResource &&other) noexcept { *this = std::move(other); }
    TextureResource &TextureResource::operator=(TextureResource &&other) noexcept
    {
        if (this != &other)
        {
            Release();
            image = std::move(other.image);
            sampler = std::exchange(other.sampler, VK_NULL_HANDLE);
            device = std::exchange(other.device, VK_NULL_HANDLE);
            descriptorSet = std::exchange(other.descriptorSet, VK_NULL_HANDLE);
            pool = std::exchange(other.pool, VK_NULL_HANDLE);
        }
        return *this;
    }

    SceneResources::~SceneResources() { Shutdown(); }

    bool SceneResources::Initialize(VkDevice device, VulkanAllocator &allocator, const VkPhysicalDeviceProperties &deviceProperties)
    {
        device_ = device;
        allocator_ = &allocator;
        maxAnisotropy_ = std::min(8.0F, deviceProperties.limits.maxSamplerAnisotropy);

        // 1. Layout for set 1: one combined image sampler read by the fragment shader.
        const VkDescriptorSetLayoutBinding binding{0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr};
        VkDescriptorSetLayoutCreateInfo layoutInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
        layoutInfo.bindingCount = 1;
        layoutInfo.pBindings = &binding;
        VkResult result = vkCreateDescriptorSetLayout(device_, &layoutInfo, nullptr, &textureSetLayout_);
        if (result != VK_SUCCESS)
        {
            LogVkFailure("Failed creating texture descriptor set layout.", result);
            return false;
        }

        // 2. Pool with one set per texture; FREE bit so single sets can be released.
        constexpr std::uint32_t maxTextures = 256;
        const VkDescriptorPoolSize poolSize{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, maxTextures};
        VkDescriptorPoolCreateInfo poolInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
        poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
        poolInfo.maxSets = maxTextures;
        poolInfo.poolSizeCount = 1;
        poolInfo.pPoolSizes = &poolSize;
        result = vkCreateDescriptorPool(device_, &poolInfo, nullptr, &texturePool_);
        if (result != VK_SUCCESS)
        {
            LogVkFailure("Failed creating texture descriptor pool.", result);
            return false;
        }

        // 3. A 1x1 white texture, drawn whenever an item has no (or a stale) texture.
        fallbackTexture_ = CreateTexture(core::TextureData{1, 1, {255, 255, 255, 255}});
        return static_cast<bool>(fallbackTexture_);
    }

    void SceneResources::Shutdown() noexcept
    {
        // Resources free their own descriptor sets, so they go before the pool.
        retired_.Flush();
        textures_.Clear();
        meshes_.Clear();
        fallbackTexture_ = {};
        if (texturePool_ != VK_NULL_HANDLE)
        {
            vkDestroyDescriptorPool(device_, texturePool_, nullptr);
            texturePool_ = VK_NULL_HANDLE;
        }
        if (textureSetLayout_ != VK_NULL_HANDLE)
        {
            vkDestroyDescriptorSetLayout(device_, textureSetLayout_, nullptr);
            textureSetLayout_ = VK_NULL_HANDLE;
        }
    }

    bool SceneResources::UploadMesh(const core::MeshData &data, MeshResource &mesh)
    {
        if (const std::string error = core::ValidateMesh(data); !error.empty())
        {
            diagnostics::Error("Rejected mesh: " + error + ".");
            return false;
        }
        if (!allocator_->CreateDeviceBuffer(data.vertices.data(), data.vertices.size() * sizeof(Vertex),
                                            VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, mesh.vertexBuffer) ||
            !allocator_->CreateDeviceBuffer(data.indices.data(), data.indices.size() * sizeof(std::uint32_t),
                                            VK_BUFFER_USAGE_INDEX_BUFFER_BIT, mesh.indexBuffer))
        {
            return false;
        }
        if (!data.skin.empty() && !allocator_->CreateDeviceBuffer(data.skin.data(), data.skin.size() * sizeof(core::SkinVertex),
                                                                  VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, mesh.skinBuffer))
        {
            return false;
        }
        mesh.indexCount = static_cast<std::uint32_t>(data.indices.size());
        mesh.baseColor = data.baseColor;
        return true;
    }

    bool SceneResources::UploadTexture(const core::TextureData &data, TextureResource &texture)
    {
        if (const std::string error = core::ValidateTexture(data); !error.empty())
        {
            diagnostics::Error("Rejected texture: " + error + ".");
            return false;
        }

        // 1. Image + view.
        if (!allocator_->CreateTexture(core::MipChain(data), texture.image))
        {
            return false;
        }

        // 2. Sampler.
        texture.device = device_;
        VkSamplerCreateInfo samplerInfo{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
        samplerInfo.magFilter = VK_FILTER_LINEAR;
        samplerInfo.minFilter = VK_FILTER_LINEAR;
        samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerInfo.anisotropyEnable = VK_TRUE;
        samplerInfo.maxAnisotropy = maxAnisotropy_;
        samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
        samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        samplerInfo.maxLod = VK_LOD_CLAMP_NONE; // use every mip level (core::MipChain)
        VkResult result = vkCreateSampler(device_, &samplerInfo, nullptr, &texture.sampler);
        if (result != VK_SUCCESS)
        {
            LogVkFailure("Failed creating texture sampler.", result);
            return false;
        }

        // 3. Descriptor set (set 1) pointing at the image and sampler.
        VkDescriptorSetAllocateInfo allocateInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
        allocateInfo.descriptorPool = texturePool_;
        allocateInfo.descriptorSetCount = 1;
        allocateInfo.pSetLayouts = &textureSetLayout_;
        result = vkAllocateDescriptorSets(device_, &allocateInfo, &texture.descriptorSet);
        if (result != VK_SUCCESS)
        {
            LogVkFailure("Failed allocating texture descriptor set.", result);
            return false;
        }
        texture.pool = texturePool_;
        const VkDescriptorImageInfo imageInfo{texture.sampler, texture.image.GetView(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
        VkWriteDescriptorSet write{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
        write.dstSet = texture.descriptorSet;
        write.descriptorCount = 1;
        write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        write.pImageInfo = &imageInfo;
        vkUpdateDescriptorSets(device_, 1, &write, 0, nullptr);
        return true;
    }

    MeshHandle SceneResources::CreateMesh(const core::MeshData &data)
    {
        MeshResource mesh;
        return UploadMesh(data, mesh) ? meshes_.Insert(std::move(mesh)) : MeshHandle{};
    }

    TextureHandle SceneResources::CreateTexture(const core::TextureData &data)
    {
        TextureResource texture;
        return UploadTexture(data, texture) ? textures_.Insert(std::move(texture)) : TextureHandle{};
    }

    bool SceneResources::ReplaceMesh(MeshHandle handle, const core::MeshData &data)
    {
        MeshResource *slot = meshes_.Get(handle);
        MeshResource fresh;
        if (slot == nullptr || !UploadMesh(data, fresh))
        {
            return false;
        }
        // Old buffers may still be read by in-flight frames: retire instead of destroying.
        retired_.Retire(std::exchange(*slot, std::move(fresh)), frameSerial_);
        return true;
    }

    bool SceneResources::ReplaceTexture(TextureHandle handle, const core::TextureData &data)
    {
        TextureResource *slot = textures_.Get(handle);
        TextureResource fresh;
        if (slot == nullptr || !UploadTexture(data, fresh))
        {
            return false;
        }
        retired_.Retire(std::exchange(*slot, std::move(fresh)), frameSerial_);
        return true;
    }

    bool SceneResources::DestroyMesh(MeshHandle handle)
    {
        MeshResource *slot = meshes_.Get(handle);
        if (slot == nullptr)
        {
            return false;
        }
        retired_.Retire(std::move(*slot), frameSerial_);
        return meshes_.Remove(handle);
    }

    bool SceneResources::DestroyTexture(TextureHandle handle)
    {
        TextureResource *slot = textures_.Get(handle);
        if (slot == nullptr || handle == fallbackTexture_)
        {
            return false;
        }
        retired_.Retire(std::move(*slot), frameSerial_);
        return textures_.Remove(handle);
    }

    void SceneResources::OnFrameSubmitted()
    {
        // After frame N is submitted, frames up to N - framesInFlight are known finished.
        ++frameSerial_;
        if (frameSerial_ >= framesInFlight)
        {
            retired_.Collect(frameSerial_ - framesInFlight);
        }
    }
}
