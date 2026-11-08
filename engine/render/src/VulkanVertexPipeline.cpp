#include "VulkanVertexPipeline.hpp"

#include <Log.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <string>
#include <vector>

namespace
{
    // Loads a SPIR-V shader from a file and creates a VkShaderModule.
    bool CreateShader(VkDevice device, const std::filesystem::path &path, VkShaderModule &shader)
    {
        // Scene.hlsl → shader compiler → SPIR-V file → VkShaderModule
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

    // Finds a supported depth format for the given physical device.
    VkFormat FindDepthFormat(VkPhysicalDevice physicalDevice)
    {
        constexpr std::array formats{VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT};
        for (VkFormat format : formats)
        {
            VkFormatProperties properties{};
            vkGetPhysicalDeviceFormatProperties(physicalDevice, format, &properties);
            if ((properties.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) != 0)
            {
                return format;
            }
        }
        return VK_FORMAT_UNDEFINED;
    }
}

namespace umbra::render
{
    VulkanVertexPipeline::~VulkanVertexPipeline() { Shutdown(); }

    bool VulkanVertexPipeline::Initialize(
        VkPhysicalDevice physicalDevice,
        VkDevice device,
        VulkanAllocator &allocator,
        const VulkanSwapchain &swapchain,
        const std::filesystem::path &shaderDirectory,
        VkDescriptorSetLayout textureSetLayout)
    {
        if (device_ != VK_NULL_HANDLE || device == VK_NULL_HANDLE || swapchain.GetImageViews().empty())
        {
            return false;
        }

        // 1. choose depth format
        device_ = device;
        depthFormat_ = FindDepthFormat(physicalDevice);
        if (depthFormat_ == VK_FORMAT_UNDEFINED)
        {
            diagnostics::Error("No supported depth format.");
            Shutdown();
            return false;
        }

        // 2. create descriptor set layout
        // set 0, binding 0: per-frame uniform buffer (camera, lights, fog)
        // set 0, binding 1: the sun's shadow map (fragment shader)
        // set 1 (owned by SceneResources): per-texture combined image sampler
        const std::array frameBindings{
            VkDescriptorSetLayoutBinding{0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, nullptr},
            VkDescriptorSetLayoutBinding{1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr}};
        VkDescriptorSetLayoutCreateInfo descriptorInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
        descriptorInfo.bindingCount = static_cast<std::uint32_t>(frameBindings.size());
        descriptorInfo.pBindings = frameBindings.data();
        if (vkCreateDescriptorSetLayout(device_, &descriptorInfo, nullptr, &descriptorSetLayout_) != VK_SUCCESS)
        {
            diagnostics::Error("Failed creating descriptor set layout.");
            Shutdown();
            return false;
        }

        // 3. create pipeline layout
        // shader-interface: two descriptor sets plus one push-constant block per draw
        const std::array setLayouts{descriptorSetLayout_, textureSetLayout};
        const VkPushConstantRange pushRange{VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(DrawConstants)};
        VkPipelineLayoutCreateInfo layoutInfo{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
        layoutInfo.setLayoutCount = static_cast<std::uint32_t>(setLayouts.size());
        layoutInfo.pSetLayouts = setLayouts.data();
        layoutInfo.pushConstantRangeCount = 1;
        layoutInfo.pPushConstantRanges = &pushRange;
        if (vkCreatePipelineLayout(device_, &layoutInfo, nullptr, &pipelineLayout_) != VK_SUCCESS)
        {
            diagnostics::Error("Failed creating pipeline layout.");
            Shutdown();
            return false;
        }

        // 4. create render pass
        // one color attachment, one depth attachment, one subpass
        // this is like a destination for the rendered image
        VkAttachmentDescription color{};
        color.format = swapchain.GetImageFormat();
        color.samples = VK_SAMPLE_COUNT_1_BIT;
        color.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        color.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        color.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        color.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        color.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        color.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        VkAttachmentDescription depth{};
        depth.format = depthFormat_;
        depth.samples = VK_SAMPLE_COUNT_1_BIT;
        depth.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        depth.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        depth.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        depth.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        depth.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        depth.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        const std::array attachments{color, depth};
        const VkAttachmentReference colorRef{0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
        const VkAttachmentReference depthRef{1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};
        VkSubpassDescription subpass{};
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount = 1;
        subpass.pColorAttachments = &colorRef;
        subpass.pDepthStencilAttachment = &depthRef;
        VkSubpassDependency dependency{};
        dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
        dependency.dstSubpass = 0;
        dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
        dependency.dstStageMask = dependency.srcStageMask;
        dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        VkRenderPassCreateInfo renderPassInfo{VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};
        renderPassInfo.attachmentCount = 2;
        renderPassInfo.pAttachments = attachments.data();
        renderPassInfo.subpassCount = 1;
        renderPassInfo.pSubpasses = &subpass;
        renderPassInfo.dependencyCount = 1;
        renderPassInfo.pDependencies = &dependency;
        if (vkCreateRenderPass(device_, &renderPassInfo, nullptr, &renderPass_) != VK_SUCCESS)
        {
            diagnostics::Error("Failed creating depth render pass.");
            Shutdown();
            return false;
        }

        // Scene, sky and shadow shaders; they are only needed until the pipelines exist.
        std::array<VkShaderModule, 5> shaders{};
        const char *shaderFiles[] = {"Scene.vert.spv", "Scene.frag.spv", "Sky.vert.spv", "Sky.frag.spv", "Shadow.vert.spv"};
        const auto destroyShaders = [&] {
            for (VkShaderModule &shader : shaders)
                if (shader)
                    vkDestroyShaderModule(device_, shader, nullptr), shader = VK_NULL_HANDLE;
        };
        for (std::size_t i = 0; i < shaders.size(); ++i)
            if (!CreateShader(device_, shaderDirectory / shaderFiles[i], shaders[i]))
            {
                destroyShaders();
                Shutdown();
                return false;
            }
        const VkShaderModule vertexShader = shaders[0], fragmentShader = shaders[1];

        // 5. create graphics pipeline
        // 5.1. map HLSL shader interface to Vulkan shader interface
        const std::array stages{
            VkPipelineShaderStageCreateInfo{VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0,
                                            VK_SHADER_STAGE_VERTEX_BIT, vertexShader, "main", nullptr},
            VkPipelineShaderStageCreateInfo{VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0,
                                            VK_SHADER_STAGE_FRAGMENT_BIT, fragmentShader, "main", nullptr}};
        const VkVertexInputBindingDescription binding{0, sizeof(Vertex), VK_VERTEX_INPUT_RATE_VERTEX};
        const std::array attributes{
            VkVertexInputAttributeDescription{0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, position)},
            VkVertexInputAttributeDescription{1, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, normal)},
            VkVertexInputAttributeDescription{2, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(Vertex, uv)}};
        VkPipelineVertexInputStateCreateInfo vertexInput{VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
        vertexInput.vertexBindingDescriptionCount = 1;
        vertexInput.pVertexBindingDescriptions = &binding;
        vertexInput.vertexAttributeDescriptionCount = static_cast<std::uint32_t>(attributes.size());
        vertexInput.pVertexAttributeDescriptions = attributes.data();

        // 5.2. configure input assembly (group 3 vertices into a triangle)
        VkPipelineInputAssemblyStateCreateInfo assembly{VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
        assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        VkViewport viewport{};
        viewport.width = static_cast<float>(swapchain.GetExtent().width);
        viewport.height = static_cast<float>(swapchain.GetExtent().height);
        viewport.maxDepth = 1.0F;

        // 5.3. configure viewport and scissor rectangle
        // viewport: defines the transformation from normalized device coordinates to swapchain coordinates
        // scissor: defines the region of the framebuffer to render to
        VkRect2D scissor{};
        scissor.extent = swapchain.GetExtent();
        VkPipelineViewportStateCreateInfo viewportInfo{VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};
        viewportInfo.viewportCount = 1;
        viewportInfo.pViewports = &viewport;
        viewportInfo.scissorCount = 1;
        viewportInfo.pScissors = &scissor;

        // 5.4. configure rasterization state (how polygons are rendered)
        // fills polygons with fragments, culls back-facing polygons, and sets the front face winding order
        VkPipelineRasterizationStateCreateInfo raster{VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
        raster.polygonMode = VK_POLYGON_MODE_FILL;
        raster.cullMode = VK_CULL_MODE_BACK_BIT;
        raster.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
        raster.lineWidth = 1.0F;
        VkPipelineMultisampleStateCreateInfo multisample{VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};
        multisample.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
        VkPipelineDepthStencilStateCreateInfo depthState{VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO};
        depthState.depthTestEnable = VK_TRUE;
        depthState.depthWriteEnable = VK_TRUE;
        depthState.depthCompareOp = VK_COMPARE_OP_LESS;
        depthState.maxDepthBounds = 1.0F;

        // 5.5. configure color blending (how fragment colors are combined with framebuffer colors)
        VkPipelineColorBlendAttachmentState blendAttachment{};
        blendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                         VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        VkPipelineColorBlendStateCreateInfo blend{VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};
        blend.attachmentCount = 1;
        blend.pAttachments = &blendAttachment;

        // 6. create graphics pipeline
        VkGraphicsPipelineCreateInfo pipelineInfo{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
        pipelineInfo.stageCount = 2;
        pipelineInfo.pStages = stages.data();
        pipelineInfo.pVertexInputState = &vertexInput;
        pipelineInfo.pInputAssemblyState = &assembly;
        pipelineInfo.pViewportState = &viewportInfo;
        pipelineInfo.pRasterizationState = &raster;
        pipelineInfo.pMultisampleState = &multisample;
        pipelineInfo.pDepthStencilState = &depthState;
        pipelineInfo.pColorBlendState = &blend;
        pipelineInfo.layout = pipelineLayout_;
        pipelineInfo.renderPass = renderPass_;
        VkResult pipelineResult = vkCreateGraphicsPipelines(device_, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipeline_);

        // 6.1. The sky: no vertex buffer (the shader makes one triangle), no depth test, drawn first.
        const std::array skyStages{
            VkPipelineShaderStageCreateInfo{VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_VERTEX_BIT, shaders[2], "main", nullptr},
            VkPipelineShaderStageCreateInfo{VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_FRAGMENT_BIT, shaders[3], "main", nullptr}};
        VkPipelineVertexInputStateCreateInfo noVertices{VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
        VkPipelineDepthStencilStateCreateInfo noDepth{VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO};
        noDepth.maxDepthBounds = 1.0F;
        VkPipelineRasterizationStateCreateInfo bothSides = raster;
        bothSides.cullMode = VK_CULL_MODE_NONE;
        VkGraphicsPipelineCreateInfo skyInfo = pipelineInfo;
        skyInfo.pStages = skyStages.data();
        skyInfo.pVertexInputState = &noVertices;
        skyInfo.pDepthStencilState = &noDepth;
        skyInfo.pRasterizationState = &bothSides;
        if (pipelineResult == VK_SUCCESS)
            pipelineResult = vkCreateGraphicsPipelines(device_, VK_NULL_HANDLE, 1, &skyInfo, nullptr, &skyPipeline_);

        // 6.2. The sun's shadow map: a depth image the scene shader samples afterwards.
        if (pipelineResult == VK_SUCCESS && !CreateShadowMap(physicalDevice, allocator, shaders[4], pipelineInfo, vertexInput, assembly, multisample, depthState))
            pipelineResult = VK_ERROR_INITIALIZATION_FAILED;
        destroyShaders();
        if (pipelineResult != VK_SUCCESS)
        {
            diagnostics::Error("Failed creating the scene, sky or shadow pipeline.");
            Shutdown();
            return false;
        }

        // 7. create per-image depth buffers and framebuffers
        depthImages_.resize(swapchain.GetImageViews().size());
        framebuffers_.reserve(swapchain.GetImageViews().size());
        for (std::size_t index = 0; index < swapchain.GetImageViews().size(); ++index)
        {
            if (!allocator.CreateDepthImage(swapchain.GetExtent(), depthFormat_, depthImages_[index]))
            {
                Shutdown();
                return false;
            }
            const std::array imageViews{swapchain.GetImageViews()[index], depthImages_[index].GetView()};
            VkFramebufferCreateInfo info{VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
            info.renderPass = renderPass_;
            info.attachmentCount = 2;
            info.pAttachments = imageViews.data();
            info.width = swapchain.GetExtent().width;
            info.height = swapchain.GetExtent().height;
            info.layers = 1;
            VkFramebuffer framebuffer = VK_NULL_HANDLE;
            if (vkCreateFramebuffer(device_, &info, nullptr, &framebuffer) != VK_SUCCESS)
            {
                diagnostics::Error("Failed creating depth framebuffer.");
                Shutdown();
                return false;
            }
            framebuffers_.push_back(framebuffer);
        }
        diagnostics::Info("Indexed textured pipeline and per-image depth buffers initialized.");
        return true;
    }

    bool VulkanVertexPipeline::CreateShadowMap(VkPhysicalDevice physicalDevice, VulkanAllocator &allocator, VkShaderModule shadowShader,
                                               const VkGraphicsPipelineCreateInfo &scene, const VkPipelineVertexInputStateCreateInfo &vertexInput,
                                               const VkPipelineInputAssemblyStateCreateInfo &assembly,
                                               const VkPipelineMultisampleStateCreateInfo &multisample,
                                               const VkPipelineDepthStencilStateCreateInfo &depthState)
    {
        // 1. A 16-bit depth image that can be drawn into and then sampled (every GPU supports this).
        constexpr VkFormat format = VK_FORMAT_D16_UNORM;
        VkFormatProperties properties{};
        vkGetPhysicalDeviceFormatProperties(physicalDevice, format, &properties);
        const bool linear = (properties.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT) != 0;
        if (!allocator.CreateDepthImage({shadowMapSize, shadowMapSize}, format, shadowImage_, VK_IMAGE_USAGE_SAMPLED_BIT))
            return false;

        // 2. A depth-only render pass that leaves the image ready for the scene shader to read.
        VkAttachmentDescription depth{};
        depth.format = format;
        depth.samples = VK_SAMPLE_COUNT_1_BIT;
        depth.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        depth.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        depth.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        depth.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        depth.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        depth.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        const VkAttachmentReference depthRef{0, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};
        VkSubpassDescription subpass{};
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.pDepthStencilAttachment = &depthRef;
        // Wait for last frame's reads before writing; make the writes visible to this frame's reads.
        std::array<VkSubpassDependency, 2> dependencies{};
        dependencies[0] = {VK_SUBPASS_EXTERNAL, 0, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
                           VK_ACCESS_SHADER_READ_BIT, VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT, 0};
        dependencies[1] = {0, VK_SUBPASS_EXTERNAL, VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                           VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT, 0};
        VkRenderPassCreateInfo passInfo{VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};
        passInfo.attachmentCount = 1;
        passInfo.pAttachments = &depth;
        passInfo.subpassCount = 1;
        passInfo.pSubpasses = &subpass;
        passInfo.dependencyCount = static_cast<std::uint32_t>(dependencies.size());
        passInfo.pDependencies = dependencies.data();
        if (vkCreateRenderPass(device_, &passInfo, nullptr, &shadowRenderPass_) != VK_SUCCESS)
            return false;
        const VkImageView view = shadowImage_.GetView();
        VkFramebufferCreateInfo framebufferInfo{VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
        framebufferInfo.renderPass = shadowRenderPass_;
        framebufferInfo.attachmentCount = 1;
        framebufferInfo.pAttachments = &view;
        framebufferInfo.width = framebufferInfo.height = shadowMapSize;
        framebufferInfo.layers = 1;
        if (vkCreateFramebuffer(device_, &framebufferInfo, nullptr, &shadowFramebuffer_) != VK_SUCCESS)
            return false;

        // 3. A comparison sampler: each lookup answers "lit or not" (filtered when the GPU allows).
        VkSamplerCreateInfo samplerInfo{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
        samplerInfo.magFilter = samplerInfo.minFilter = linear ? VK_FILTER_LINEAR : VK_FILTER_NEAREST;
        samplerInfo.addressModeU = samplerInfo.addressModeV = samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerInfo.compareEnable = VK_TRUE;
        samplerInfo.compareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
        samplerInfo.maxLod = 0.0F;
        if (vkCreateSampler(device_, &samplerInfo, nullptr, &shadowSampler_) != VK_SUCCESS)
            return false;

        // 4. The pipeline: vertex shader only, both sides, a small depth bias against shadow acne.
        const VkPipelineShaderStageCreateInfo stage{VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_VERTEX_BIT, shadowShader, "main", nullptr};
        VkViewport viewport{0.0F, 0.0F, static_cast<float>(shadowMapSize), static_cast<float>(shadowMapSize), 0.0F, 1.0F};
        VkRect2D scissor{{0, 0}, {shadowMapSize, shadowMapSize}};
        VkPipelineViewportStateCreateInfo viewportInfo{VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};
        viewportInfo.viewportCount = 1;
        viewportInfo.pViewports = &viewport;
        viewportInfo.scissorCount = 1;
        viewportInfo.pScissors = &scissor;
        VkPipelineRasterizationStateCreateInfo raster{VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
        raster.polygonMode = VK_POLYGON_MODE_FILL;
        raster.cullMode = VK_CULL_MODE_NONE;
        raster.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
        raster.depthBiasEnable = VK_TRUE;
        raster.depthBiasConstantFactor = 1.25F;
        raster.depthBiasSlopeFactor = 1.75F;
        raster.lineWidth = 1.0F;
        VkPipelineColorBlendStateCreateInfo noColor{VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};
        VkGraphicsPipelineCreateInfo info = scene;
        info.stageCount = 1;
        info.pStages = &stage;
        info.pVertexInputState = &vertexInput;
        info.pInputAssemblyState = &assembly;
        info.pViewportState = &viewportInfo;
        info.pRasterizationState = &raster;
        info.pMultisampleState = &multisample;
        info.pDepthStencilState = &depthState;
        info.pColorBlendState = &noColor;
        info.renderPass = shadowRenderPass_;
        return vkCreateGraphicsPipelines(device_, VK_NULL_HANDLE, 1, &info, nullptr, &shadowPipeline_) == VK_SUCCESS;
    }

    void VulkanVertexPipeline::Shutdown() noexcept
    {
        if (device_ == VK_NULL_HANDLE)
        {
            return;
        }
        for (VkFramebuffer framebuffer : framebuffers_)
        {
            vkDestroyFramebuffer(device_, framebuffer, nullptr);
        }
        framebuffers_.clear();
        depthImages_.clear();
        if (shadowFramebuffer_)
            vkDestroyFramebuffer(device_, shadowFramebuffer_, nullptr);
        if (shadowPipeline_)
            vkDestroyPipeline(device_, shadowPipeline_, nullptr);
        if (shadowRenderPass_)
            vkDestroyRenderPass(device_, shadowRenderPass_, nullptr);
        if (shadowSampler_)
            vkDestroySampler(device_, shadowSampler_, nullptr);
        if (skyPipeline_)
            vkDestroyPipeline(device_, skyPipeline_, nullptr);
        shadowImage_.Reset();
        shadowFramebuffer_ = VK_NULL_HANDLE;
        shadowPipeline_ = VK_NULL_HANDLE;
        shadowRenderPass_ = VK_NULL_HANDLE;
        shadowSampler_ = VK_NULL_HANDLE;
        skyPipeline_ = VK_NULL_HANDLE;
        if (pipeline_)
        {
            vkDestroyPipeline(device_, pipeline_, nullptr);
        }
        if (renderPass_)
        {
            vkDestroyRenderPass(device_, renderPass_, nullptr);
        }
        if (pipelineLayout_)
        {
            vkDestroyPipelineLayout(device_, pipelineLayout_, nullptr);
        }
        if (descriptorSetLayout_)
        {
            vkDestroyDescriptorSetLayout(device_, descriptorSetLayout_, nullptr);
        }
        pipeline_ = VK_NULL_HANDLE;
        renderPass_ = VK_NULL_HANDLE;
        pipelineLayout_ = VK_NULL_HANDLE;
        descriptorSetLayout_ = VK_NULL_HANDLE;
        depthFormat_ = VK_FORMAT_UNDEFINED;
        device_ = VK_NULL_HANDLE;
    }
}
