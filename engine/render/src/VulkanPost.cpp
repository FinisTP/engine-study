#include "VulkanPost.hpp"

#include <Log.hpp>

#include <algorithm>
#include <array>

namespace umbra::render
{
    namespace
    {
        constexpr VkFormat hdrFormat = VK_FORMAT_R16G16B16A16_SFLOAT;

        // A color render pass: clear or keep what is there, then leave it ready for sampling.
        VkRenderPass MakePass(VkDevice device, VkFormat color, VkAttachmentLoadOp load, VkFormat depth = VK_FORMAT_UNDEFINED)
        {
            std::array<VkAttachmentDescription, 2> attachments{};
            attachments[0].format = color;
            attachments[0].samples = VK_SAMPLE_COUNT_1_BIT;
            attachments[0].loadOp = load;
            attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
            attachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
            attachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
            attachments[0].initialLayout = load == VK_ATTACHMENT_LOAD_OP_LOAD ? VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL : VK_IMAGE_LAYOUT_UNDEFINED;
            attachments[0].finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            attachments[1] = attachments[0];
            attachments[1].format = depth;
            attachments[1].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
            attachments[1].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
            attachments[1].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            attachments[1].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
            const VkAttachmentReference colorRef{0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
            const VkAttachmentReference depthRef{1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};
            VkSubpassDescription subpass{};
            subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
            subpass.colorAttachmentCount = 1;
            subpass.pColorAttachments = &colorRef;
            subpass.pDepthStencilAttachment = depth != VK_FORMAT_UNDEFINED ? &depthRef : nullptr;
            // 1. Wait until earlier reads (last use of this picture) and writes are done.
            // 2. Later passes read the result in their fragment shader.
            std::array<VkSubpassDependency, 2> dependencies{};
            dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
            dependencies[0].dstSubpass = 0;
            dependencies[0].srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
            dependencies[0].srcAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
            dependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
            dependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
            dependencies[1].srcSubpass = 0;
            dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
            dependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
            dependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
            dependencies[1].dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
            dependencies[1].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
            VkRenderPassCreateInfo info{VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};
            info.attachmentCount = depth != VK_FORMAT_UNDEFINED ? 2 : 1;
            info.pAttachments = attachments.data();
            info.subpassCount = 1;
            info.pSubpasses = &subpass;
            info.dependencyCount = 2;
            info.pDependencies = dependencies.data();
            VkRenderPass pass = VK_NULL_HANDLE;
            return vkCreateRenderPass(device, &info, nullptr, &pass) == VK_SUCCESS ? pass : VK_NULL_HANDLE;
        }

        // The screen pass: the same attachments, subpass and dependency as the swapchain's pass
        // (VulkanVertexPipeline), so pipelines made for one draw in the other ("compatible" passes);
        // only the color's final layout differs: ready to be sampled by the analog screen.
        VkRenderPass MakeScreenPass(VkDevice device, VkFormat color, VkFormat depth)
        {
            std::array<VkAttachmentDescription, 2> attachments{};
            attachments[0].format = color;
            attachments[0].samples = VK_SAMPLE_COUNT_1_BIT;
            attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
            attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
            attachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
            attachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
            attachments[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            attachments[0].finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            attachments[1] = attachments[0];
            attachments[1].format = depth;
            attachments[1].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
            attachments[1].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
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
            VkRenderPassCreateInfo info{VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};
            info.attachmentCount = 2;
            info.pAttachments = attachments.data();
            info.subpassCount = 1;
            info.pSubpasses = &subpass;
            info.dependencyCount = 1;
            info.pDependencies = &dependency;
            VkRenderPass pass = VK_NULL_HANDLE;
            return vkCreateRenderPass(device, &info, nullptr, &pass) == VK_SUCCESS ? pass : VK_NULL_HANDLE;
        }

        VkFramebuffer MakeFramebuffer(VkDevice device, VkRenderPass pass, VkExtent2D size, std::initializer_list<VkImageView> views)
        {
            const std::vector<VkImageView> list(views);
            VkFramebufferCreateInfo info{VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
            info.renderPass = pass;
            info.attachmentCount = static_cast<std::uint32_t>(list.size());
            info.pAttachments = list.data();
            info.width = size.width;
            info.height = size.height;
            info.layers = 1;
            VkFramebuffer framebuffer = VK_NULL_HANDLE;
            return vkCreateFramebuffer(device, &info, nullptr, &framebuffer) == VK_SUCCESS ? framebuffer : VK_NULL_HANDLE;
        }
    }

    VulkanPost::~VulkanPost() { Shutdown(); }

    VkExtent2D VulkanPost::LevelSize(std::size_t level) const
    {
        const std::uint32_t divisor = static_cast<std::uint32_t>(core::BloomStartDivisor(quality_)) << level;
        return {std::max(extent_.width / divisor, 1u), std::max(extent_.height / divisor, 1u)};
    }

    bool VulkanPost::Initialize(VkDevice device, VulkanAllocator &allocator, const VulkanSwapchain &swapchain, VkFormat depthFormat,
                                const std::filesystem::path &shaderDirectory, core::PostQuality quality)
    {
        device_ = device;
        allocator_ = &allocator;
        extent_ = swapchain.GetExtent();
        quality_ = quality;
        const auto fail = [&](const char *what) {
            diagnostics::Error(std::string{"Post-processing: "} + what);
            Shutdown();
            return false;
        };

        // 1. Render passes: the scene (HDR + depth), a bloom level from scratch, a bloom level added onto.
        scenePass_ = MakePass(device_, hdrFormat, VK_ATTACHMENT_LOAD_OP_CLEAR, depthFormat);
        bloomPass_ = MakePass(device_, hdrFormat, VK_ATTACHMENT_LOAD_OP_DONT_CARE);
        addPass_ = MakePass(device_, hdrFormat, VK_ATTACHMENT_LOAD_OP_LOAD);
        screenPass_ = MakeScreenPass(device_, swapchain.GetImageFormat(), depthFormat);
        if (!scenePass_ || !bloomPass_ || !addPass_ || !screenPass_)
            return fail("render passes");

        // 2. Descriptors: source picture, bloom picture, color table; constants pushed per pass.
        std::array<VkDescriptorSetLayoutBinding, 3> bindings{};
        for (std::uint32_t i = 0; i < bindings.size(); ++i)
            bindings[i] = {i, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr};
        VkDescriptorSetLayoutCreateInfo setInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
        setInfo.bindingCount = static_cast<std::uint32_t>(bindings.size());
        setInfo.pBindings = bindings.data();
        const VkPushConstantRange push{VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(core::PostBlock)};
        VkPipelineLayoutCreateInfo layoutInfo{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
        layoutInfo.setLayoutCount = 1;
        layoutInfo.pSetLayouts = &setLayout_;
        layoutInfo.pushConstantRangeCount = 1;
        layoutInfo.pPushConstantRanges = &push;
        if (vkCreateDescriptorSetLayout(device_, &setInfo, nullptr, &setLayout_) != VK_SUCCESS ||
            vkCreatePipelineLayout(device_, &layoutInfo, nullptr, &layout_) != VK_SUCCESS)
            return fail("layouts");
        VkSamplerCreateInfo samplerInfo{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
        samplerInfo.magFilter = samplerInfo.minFilter = VK_FILTER_LINEAR;
        samplerInfo.addressModeU = samplerInfo.addressModeV = samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerInfo.maxLod = 0.0F;
        if (vkCreateSampler(device_, &samplerInfo, nullptr, &sampler_) != VK_SUCCESS)
            return fail("sampler");

        // 3. Pictures per swapchain image: HDR + depth for the scene, then the bloom levels.
        const std::size_t levels = static_cast<std::size_t>(core::BloomLevels(quality_));
        targets_.resize(swapchain.GetImageViews().size());
        for (Target &t : targets_)
        {
            if (!allocator.CreateColorTarget(extent_, hdrFormat, t.hdr) || !allocator.CreateDepthImage(extent_, depthFormat, t.depth))
                return fail("HDR picture");
            t.sceneFramebuffer = MakeFramebuffer(device_, scenePass_, extent_, {t.hdr.GetView(), t.depth.GetView()});
            // The screen picture for the analog screen; its pass reuses the scene's depth (done with by then).
            if (!allocator.CreateColorTarget(extent_, swapchain.GetImageFormat(), t.screen))
                return fail("screen picture");
            t.screenFramebuffer = MakeFramebuffer(device_, screenPass_, extent_, {t.screen.GetView(), t.depth.GetView()});
            t.levels.resize(levels);
            for (std::size_t i = 0; i < levels; ++i)
            {
                if (!allocator.CreateColorTarget(LevelSize(i), hdrFormat, t.levels[i]))
                    return fail("bloom picture");
                t.levelFramebuffers.push_back(MakeFramebuffer(device_, bloomPass_, LevelSize(i), {t.levels[i].GetView()}));
            }
            if (!t.sceneFramebuffer || !t.screenFramebuffer || std::count(t.levelFramebuffers.begin(), t.levelFramebuffers.end(), VK_NULL_HANDLE) > 0)
                return fail("framebuffers");
        }

        // 4. One descriptor set per pass: `levels` down, `levels - 1` up, 1 composite, 1 analog.
        const std::uint32_t sets = static_cast<std::uint32_t>(targets_.size() * (levels * 2 + 1));
        const VkDescriptorPoolSize poolSize{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, sets * 3};
        VkDescriptorPoolCreateInfo poolInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
        poolInfo.maxSets = sets;
        poolInfo.poolSizeCount = 1;
        poolInfo.pPoolSizes = &poolSize;
        if (vkCreateDescriptorPool(device_, &poolInfo, nullptr, &pool_) != VK_SUCCESS)
            return fail("descriptor pool");
        const auto allocate = [&]() {
            VkDescriptorSetAllocateInfo info{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
            info.descriptorPool = pool_;
            info.descriptorSetCount = 1;
            info.pSetLayouts = &setLayout_;
            VkDescriptorSet set = VK_NULL_HANDLE;
            vkAllocateDescriptorSets(device_, &info, &set);
            return set;
        };
        for (Target &t : targets_)
        {
            for (std::size_t i = 0; i < levels; ++i)
                t.downSets.push_back(allocate());
            for (std::size_t i = 0; i + 1 < levels; ++i)
                t.upSets.push_back(allocate());
            t.compositeSet = allocate();
            t.analogSet = allocate();
        }

        // 5. A neutral color table until the scene sets a grade.
        if (!allocator.CreateLut3D(core::BakeGradeLut(grade_), lut_))
            return fail("color table");
        WriteSets();

        // 6. Bloom pipelines: one screen triangle, a fragment shader each, viewport set per pass.
        if (!LoadShaderModule(device_, shaderDirectory / "Fullscreen.vert.spv", fullscreen_) ||
            !LoadShaderModule(device_, shaderDirectory / "Composite.frag.spv", compositeShader_) ||
            !LoadShaderModule(device_, shaderDirectory / "Analog.frag.spv", analogShader_))
            return fail("shaders");
        const auto make = [&](const char *file, VkRenderPass pass, bool add) {
            VkShaderModule fragment = VK_NULL_HANDLE;
            if (!LoadShaderModule(device_, shaderDirectory / file, fragment))
                return VkPipeline{VK_NULL_HANDLE};
            const VkPipeline pipeline = MakePipeline(fragment, pass, add);
            vkDestroyShaderModule(device_, fragment, nullptr);
            return pipeline;
        };
        bright_ = make("Bright.frag.spv", bloomPass_, false);
        down_ = make("Down.frag.spv", bloomPass_, false);
        up_ = make("Up.frag.spv", addPass_, true);
        if (!bright_ || !down_ || !up_)
            return fail("bloom pipelines");
        return true;
    }

    VkPipeline VulkanPost::MakePipeline(VkShaderModule fragment, VkRenderPass pass, bool add) const
    {
        const std::array stages{
            VkPipelineShaderStageCreateInfo{VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_VERTEX_BIT, fullscreen_, "main", nullptr},
            VkPipelineShaderStageCreateInfo{VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_FRAGMENT_BIT, fragment, "main", nullptr}};
        VkPipelineVertexInputStateCreateInfo noVertices{VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
        VkPipelineInputAssemblyStateCreateInfo assembly{VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
        assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        VkPipelineViewportStateCreateInfo viewport{VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};
        viewport.viewportCount = viewport.scissorCount = 1;
        VkPipelineRasterizationStateCreateInfo raster{VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
        raster.cullMode = VK_CULL_MODE_NONE;
        raster.lineWidth = 1.0F;
        VkPipelineMultisampleStateCreateInfo multisample{VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};
        multisample.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
        VkPipelineDepthStencilStateCreateInfo noDepth{VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO};
        noDepth.maxDepthBounds = 1.0F;
        VkPipelineColorBlendAttachmentState blendAttachment{};
        blendAttachment.colorWriteMask = 0xF;
        if (add) // ONE + ONE: the smaller level's light is added onto this one
        {
            blendAttachment.blendEnable = VK_TRUE;
            blendAttachment.srcColorBlendFactor = blendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE;
            blendAttachment.srcAlphaBlendFactor = blendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        }
        VkPipelineColorBlendStateCreateInfo blend{VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};
        blend.attachmentCount = 1;
        blend.pAttachments = &blendAttachment;
        const std::array dynamicStates{VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
        VkPipelineDynamicStateCreateInfo dynamic{VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};
        dynamic.dynamicStateCount = static_cast<std::uint32_t>(dynamicStates.size());
        dynamic.pDynamicStates = dynamicStates.data();
        VkGraphicsPipelineCreateInfo info{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
        info.stageCount = 2;
        info.pStages = stages.data();
        info.pVertexInputState = &noVertices;
        info.pInputAssemblyState = &assembly;
        info.pViewportState = &viewport;
        info.pRasterizationState = &raster;
        info.pMultisampleState = &multisample;
        info.pDepthStencilState = &noDepth;
        info.pColorBlendState = &blend;
        info.pDynamicState = &dynamic;
        info.layout = layout_;
        info.renderPass = pass;
        VkPipeline pipeline = VK_NULL_HANDLE;
        if (vkCreateGraphicsPipelines(device_, VK_NULL_HANDLE, 1, &info, nullptr, &pipeline) != VK_SUCCESS)
            pipeline = VK_NULL_HANDLE;
        return pipeline;
    }

    bool VulkanPost::CreateComposite(VkRenderPass finalPass)
    {
        composite_ = compositeShader_ ? MakePipeline(compositeShader_, finalPass, false) : VK_NULL_HANDLE;
        analog_ = analogShader_ ? MakePipeline(analogShader_, finalPass, false) : VK_NULL_HANDLE;
        if (!composite_ || !analog_)
            diagnostics::Error("Post-processing: composite or analog pipeline");
        return composite_ != VK_NULL_HANDLE && analog_ != VK_NULL_HANDLE;
    }

    void VulkanPost::WriteSets()
    {
        // Every set names all three bindings; unused ones point at real pictures so they stay valid.
        const auto write = [&](VkDescriptorSet set, VkImageView source, VkImageView bloom) {
            const std::array images{VkDescriptorImageInfo{sampler_, source, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL},
                                    VkDescriptorImageInfo{sampler_, bloom, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL},
                                    VkDescriptorImageInfo{sampler_, lut_.GetView(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL}};
            std::array<VkWriteDescriptorSet, 3> writes{};
            for (std::uint32_t i = 0; i < writes.size(); ++i)
            {
                writes[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                writes[i].dstSet = set;
                writes[i].dstBinding = i;
                writes[i].descriptorCount = 1;
                writes[i].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
                writes[i].pImageInfo = &images[i];
            }
            vkUpdateDescriptorSets(device_, static_cast<std::uint32_t>(writes.size()), writes.data(), 0, nullptr);
        };
        for (Target &t : targets_)
        {
            for (std::size_t i = 0; i < t.downSets.size(); ++i)
            {
                const VkImageView from = i == 0 ? t.hdr.GetView() : t.levels[i - 1].GetView();
                write(t.downSets[i], from, from);
            }
            for (std::size_t i = 0; i < t.upSets.size(); ++i)
                write(t.upSets[i], t.levels[i + 1].GetView(), t.levels[i + 1].GetView());
            write(t.compositeSet, t.hdr.GetView(), t.levels[0].GetView());
            write(t.analogSet, t.screen.GetView(), t.screen.GetView());
        }
    }

    void VulkanPost::SetGrade(const core::PostSettings &settings, const std::shared_ptr<const core::TextureData> &lutImage)
    {
        const bool same = settings.temperature == grade_.temperature && settings.saturation == grade_.saturation &&
                          settings.contrast == grade_.contrast && lutImage == gradeImage_;
        if (same || device_ == VK_NULL_HANDLE)
            return;
        // The old table may still be read by frames in flight: wait, swap, rewrite the sets.
        vkDeviceWaitIdle(device_);
        GpuImage fresh;
        if (!allocator_->CreateLut3D(core::BakeGradeLut(settings, lutImage.get()), fresh))
        {
            diagnostics::Error("Post-processing: could not build the color table; keeping the old one.");
            return;
        }
        lut_ = std::move(fresh);
        grade_ = settings;
        gradeImage_ = lutImage;
        WriteSets();
    }

    void VulkanPost::Pass(VkCommandBuffer commandBuffer, VkRenderPass pass, VkFramebuffer framebuffer, VkExtent2D size, VkPipeline pipeline,
                          VkDescriptorSet set, const core::PostBlock &block) const
    {
        if (pass != VK_NULL_HANDLE)
        {
            VkRenderPassBeginInfo begin{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
            begin.renderPass = pass;
            begin.framebuffer = framebuffer;
            begin.renderArea.extent = size;
            vkCmdBeginRenderPass(commandBuffer, &begin, VK_SUBPASS_CONTENTS_INLINE);
        }
        const VkViewport viewport{0.0F, 0.0F, static_cast<float>(size.width), static_cast<float>(size.height), 0.0F, 1.0F};
        const VkRect2D scissor{{0, 0}, size};
        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
        vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
        vkCmdSetScissor(commandBuffer, 0, 1, &scissor);
        vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, layout_, 0, 1, &set, 0, nullptr);
        vkCmdPushConstants(commandBuffer, layout_, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(block), &block);
        vkCmdDraw(commandBuffer, 3, 1, 0, 0);
        if (pass != VK_NULL_HANDLE)
            vkCmdEndRenderPass(commandBuffer);
    }

    void VulkanPost::RecordBloom(VkCommandBuffer commandBuffer, std::uint32_t imageIndex, const core::PostSettings &settings) const
    {
        const Target &t = targets_[imageIndex];
        // 1. Down: the bright light into level 0, then each level halves the one before.
        for (std::size_t i = 0; i < t.levels.size(); ++i)
        {
            const VkExtent2D from = i == 0 ? extent_ : LevelSize(i - 1);
            Pass(commandBuffer, bloomPass_, t.levelFramebuffers[i], LevelSize(i), i == 0 ? bright_ : down_, t.downSets[i],
                 core::PackPost(settings, from.width, from.height));
        }
        // 2. Up: from the smallest, each level is blurred onto the next bigger one.
        for (std::size_t i = t.levels.size() - 1; i > 0; --i)
            Pass(commandBuffer, addPass_, t.levelFramebuffers[i - 1], LevelSize(i - 1), up_, t.upSets[i - 1],
                 core::PackPost(settings, LevelSize(i).width, LevelSize(i).height));
    }

    void VulkanPost::DrawComposite(VkCommandBuffer commandBuffer, std::uint32_t imageIndex, const core::PostSettings &settings) const
    {
        Pass(commandBuffer, VK_NULL_HANDLE, VK_NULL_HANDLE, extent_, composite_, targets_[imageIndex].compositeSet,
             core::PackPost(settings, extent_.width, extent_.height));
    }

    void VulkanPost::DrawAnalog(VkCommandBuffer commandBuffer, std::uint32_t imageIndex, const core::PostSettings &settings) const
    {
        Pass(commandBuffer, VK_NULL_HANDLE, VK_NULL_HANDLE, extent_, analog_, targets_[imageIndex].analogSet,
             core::PackAnalog(settings, extent_.width, extent_.height, seconds_));
    }

    void VulkanPost::Shutdown() noexcept
    {
        if (device_ == VK_NULL_HANDLE)
            return;
        for (Target &t : targets_)
        {
            for (VkFramebuffer f : {t.sceneFramebuffer, t.screenFramebuffer})
                if (f)
                    vkDestroyFramebuffer(device_, f, nullptr);
            for (VkFramebuffer f : t.levelFramebuffers)
                if (f)
                    vkDestroyFramebuffer(device_, f, nullptr);
        }
        targets_.clear();
        lut_.Reset();
        for (VkPipeline p : {bright_, down_, up_, composite_, analog_})
            if (p)
                vkDestroyPipeline(device_, p, nullptr);
        bright_ = down_ = up_ = composite_ = analog_ = VK_NULL_HANDLE;
        for (VkShaderModule m : {fullscreen_, compositeShader_, analogShader_})
            if (m)
                vkDestroyShaderModule(device_, m, nullptr);
        fullscreen_ = compositeShader_ = analogShader_ = VK_NULL_HANDLE;
        if (pool_)
            vkDestroyDescriptorPool(device_, pool_, nullptr);
        if (sampler_)
            vkDestroySampler(device_, sampler_, nullptr);
        if (layout_)
            vkDestroyPipelineLayout(device_, layout_, nullptr);
        if (setLayout_)
            vkDestroyDescriptorSetLayout(device_, setLayout_, nullptr);
        for (VkRenderPass p : {scenePass_, bloomPass_, addPass_, screenPass_})
            if (p)
                vkDestroyRenderPass(device_, p, nullptr);
        pool_ = VK_NULL_HANDLE;
        sampler_ = VK_NULL_HANDLE;
        layout_ = VK_NULL_HANDLE;
        setLayout_ = VK_NULL_HANDLE;
        scenePass_ = bloomPass_ = addPass_ = screenPass_ = VK_NULL_HANDLE;
        grade_ = {};
        gradeImage_.reset();
        device_ = VK_NULL_HANDLE;
    }
}
