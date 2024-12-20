//
// Created by theo on 02/10/2024.
//

#include <ComputeApp.h>
#include <ComputeAppConfig.h>
#include <Common.h>

#include <GLFW/glfw3.h>
#include <imgui.h>
#include <imgui_impl_vulkan.h>

#include <algorithm>

// Generated by shader compilation
// Avoid loading shaders through the filesystem, because i'm lazy
#include <PipelineBuilder.h>
#include <Shaders/DrawToSDFTexture.h>
#include <Shaders/FillTextureFloat4.h>
#include <Shaders/FinalPass.h>
#include <Shaders/RaymarchSDF.h>
#include <Shaders/MergeCascades.h>
#include <Shaders/BuildGITexture.h>

#define MAX_LEVEL 10

class ComputeAppImpl : public ComputeApp {
public:
    ComputeAppImpl() = default;

    // TODO FIX ALIGNMENT ISSUES
    struct ComputeDrawToSDFTexturePushConstant {
        uint16_t mousePosX;
        uint16_t mousePosY;
        uint8_t radius;
        uint8_t r;
        uint8_t g;
        uint8_t b;
    };

    // TODO FIX ALIGNMENT ISSUES
    struct FillTextureFloat4PushConstant {
        float r;
        float g;
        float b;
        float a;
    };

    struct RadianceCascadeSettings {
        uint32_t maxLevel;
        uint32_t verticalProbeCountAtMaxLevel;
        float radius;
        float radiusMultiplier;
        float raymarchStepSize;
        float attenuation;
    };

    struct RaymarchPushConstant {
        RadianceCascadeSettings radianceCascadeSettings;
        uint32_t currentLevel;
    };

    struct MergeCascadesPushConstant {
        RadianceCascadeSettings radianceCascadeSettings;
        uint32_t outputLevel;
    };

    void Init() override {
        VkImageCreateInfo imgCreateInfo{};
        imgCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imgCreateInfo.imageType = VK_IMAGE_TYPE_2D;
        imgCreateInfo.extent.width = WINDOW_WIDTH;
        imgCreateInfo.extent.height = WINDOW_HEIGHT;
        imgCreateInfo.extent.depth = 1;
        imgCreateInfo.mipLevels = 1;
        imgCreateInfo.arrayLayers = 1;
        imgCreateInfo.format = VK_FORMAT_R32G32B32A32_SFLOAT;

        imgCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        imgCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        imgCreateInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
        imgCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;

        sdfImage = CreateImage(device, imgCreateInfo, allocator);
        displayImage = CreateImage(device, imgCreateInfo, allocator);

        VkSamplerCreateInfo sdfSamplerCreateInfo{};
        sdfSamplerCreateInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        sdfSamplerCreateInfo.magFilter = VK_FILTER_NEAREST; //VK_FILTER_LINEAR;
        sdfSamplerCreateInfo.minFilter = VK_FILTER_NEAREST; //VK_FILTER_LINEAR;
        sdfSamplerCreateInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST; // VK_SAMPLER_MIPMAP_MODE_LINEAR;
        sdfSamplerCreateInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        sdfSamplerCreateInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        sdfSamplerCreateInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        sdfSamplerCreateInfo.mipLodBias = 0.0f;
        sdfSamplerCreateInfo.anisotropyEnable = VK_FALSE;
        sdfSamplerCreateInfo.maxAnisotropy = 1.0f;
        sdfSamplerCreateInfo.compareEnable = VK_FALSE;
        sdfSamplerCreateInfo.compareOp = VK_COMPARE_OP_ALWAYS;
        sdfSamplerCreateInfo.minLod = 0.0f;
        sdfSamplerCreateInfo.maxLod = 0.0f;
        sdfSamplerCreateInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
        sdfSamplerCreateInfo.unnormalizedCoordinates = VK_FALSE;

        VK_CHECK(vkCreateSampler(device, &sdfSamplerCreateInfo, nullptr, &linearSampler));

        PipelineBuilder pipelineBuilder(device);

        pipelineBuilder.AddShaderStage(DrawToSDFTexture, sizeof(DrawToSDFTexture), VK_SHADER_STAGE_COMPUTE_BIT);

        // Draw to SDF pipeline
        VkDescriptorSetLayoutBinding drawToSDFTextureDescriptorSetLayoutBinding{};
        drawToSDFTextureDescriptorSetLayoutBinding.binding = 0;
        drawToSDFTextureDescriptorSetLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        drawToSDFTextureDescriptorSetLayoutBinding.descriptorCount = 1;
        drawToSDFTextureDescriptorSetLayoutBinding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
        drawToSDFTextureDescriptorSetLayoutBinding.pImmutableSamplers = nullptr;
        pipelineBuilder.AddBinding(0, drawToSDFTextureDescriptorSetLayoutBinding);

        pipelineBuilder.SetPipelineType(Pipeline::COMPUTE);

        pipelineBuilder.SetPushConstantSize<ComputeDrawToSDFTexturePushConstant>(VK_SHADER_STAGE_COMPUTE_BIT);

        drawToSDFTexturePipeline = pipelineBuilder.Build();

        // Set descriptor set
        VkDescriptorImageInfo descriptorImageInfoSDFImage{};
        descriptorImageInfoSDFImage.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
        descriptorImageInfoSDFImage.imageView = sdfImage.view;
        descriptorImageInfoSDFImage.sampler = VK_NULL_HANDLE;
        drawToSDFTexturePipeline.WriteToDescriptorSet(0, 0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                                                      &descriptorImageInfoSDFImage, nullptr);

        pipelineBuilder.Reset();

        pipelineBuilder.AddShaderStage(FillTextureFloat4, sizeof(FillTextureFloat4), VK_SHADER_STAGE_COMPUTE_BIT);

        // Draw to SDF pipeline
        VkDescriptorSetLayoutBinding fillTextureFloat4DescriptorSetLayoutBinding{};
        fillTextureFloat4DescriptorSetLayoutBinding.binding = 0;
        fillTextureFloat4DescriptorSetLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        fillTextureFloat4DescriptorSetLayoutBinding.descriptorCount = 1;
        fillTextureFloat4DescriptorSetLayoutBinding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
        fillTextureFloat4DescriptorSetLayoutBinding.pImmutableSamplers = nullptr;
        pipelineBuilder.AddBinding(0, fillTextureFloat4DescriptorSetLayoutBinding);

        pipelineBuilder.SetPipelineType(Pipeline::COMPUTE);
        pipelineBuilder.SetPushConstantSize<FillTextureFloat4PushConstant>(VK_SHADER_STAGE_COMPUTE_BIT);

        fillTextureFloat4Pipeline = pipelineBuilder.Build();

        fillTextureFloat4Pipeline.WriteToDescriptorSet(0, 0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                                                       &descriptorImageInfoSDFImage, nullptr);

        pipelineBuilder.Reset();

        pipelineBuilder.AddShaderStage(FinalPass, sizeof(FinalPass), VK_SHADER_STAGE_COMPUTE_BIT);

        VkDescriptorSetLayoutBinding convertSDFDescriptorSetLayoutBinding{};
        convertSDFDescriptorSetLayoutBinding.binding = 0;
        convertSDFDescriptorSetLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        convertSDFDescriptorSetLayoutBinding.descriptorCount = 1;
        convertSDFDescriptorSetLayoutBinding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
        convertSDFDescriptorSetLayoutBinding.pImmutableSamplers = nullptr;
        pipelineBuilder.AddBinding(0, convertSDFDescriptorSetLayoutBinding);
        convertSDFDescriptorSetLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        convertSDFDescriptorSetLayoutBinding.binding = 1;
        pipelineBuilder.AddBinding(0, convertSDFDescriptorSetLayoutBinding);
        convertSDFDescriptorSetLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        convertSDFDescriptorSetLayoutBinding.binding = 2;
        pipelineBuilder.AddBinding(0, convertSDFDescriptorSetLayoutBinding);

        pipelineBuilder.SetPipelineType(Pipeline::COMPUTE);

        finalPassPipeline = pipelineBuilder.Build();

        finalPassPipeline.WriteToDescriptorSet(0, 0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, &descriptorImageInfoSDFImage,
                                               nullptr);

        VkDescriptorImageInfo descriptorImageInfoDisplayImage{};
        descriptorImageInfoDisplayImage.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
        descriptorImageInfoDisplayImage.imageView = displayImage.view;
        descriptorImageInfoDisplayImage.sampler = VK_NULL_HANDLE;

        finalPassPipeline.WriteToDescriptorSet(0, 2, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, &descriptorImageInfoDisplayImage,
                                               nullptr);

        pipelineBuilder.Reset();

        pipelineBuilder.AddShaderStage(RaymarchSDF, sizeof(RaymarchSDF), VK_SHADER_STAGE_COMPUTE_BIT);

        VkDescriptorSetLayoutBinding raymarchDescriptorSetLayoutBinding{};
        raymarchDescriptorSetLayoutBinding.binding = 0;
        raymarchDescriptorSetLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        raymarchDescriptorSetLayoutBinding.descriptorCount = 1;
        raymarchDescriptorSetLayoutBinding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
        raymarchDescriptorSetLayoutBinding.pImmutableSamplers = nullptr;
        pipelineBuilder.AddBinding(0, raymarchDescriptorSetLayoutBinding);
        raymarchDescriptorSetLayoutBinding.binding = 1;
        raymarchDescriptorSetLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        pipelineBuilder.AddBinding(0, raymarchDescriptorSetLayoutBinding);

        pipelineBuilder.SetPipelineType(Pipeline::COMPUTE);
        pipelineBuilder.SetPushConstantSize<RaymarchPushConstant>(VK_SHADER_STAGE_COMPUTE_BIT);

        // Create sampler

        VkDescriptorImageInfo descriptorImageInfoSDFImageSampler{};
        descriptorImageInfoSDFImageSampler.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
        descriptorImageInfoSDFImageSampler.imageView = sdfImage.view;
        descriptorImageInfoSDFImageSampler.sampler = linearSampler;
        for (int i = 0; i < MAX_LEVEL; i++) {
            raymarchPipelines.push_back(pipelineBuilder.Build());
            raymarchPipelines.back().WriteToDescriptorSet(0, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                                          &descriptorImageInfoSDFImageSampler, nullptr);
        }

        pipelineBuilder.Reset();

        pipelineBuilder.AddShaderStage(MergeCascades, sizeof(MergeCascades), VK_SHADER_STAGE_COMPUTE_BIT);

        VkDescriptorSetLayoutBinding mergeCascadeDescriptorSetLayoutBinding{};
        mergeCascadeDescriptorSetLayoutBinding.binding = 0;
        mergeCascadeDescriptorSetLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        mergeCascadeDescriptorSetLayoutBinding.descriptorCount = 1;
        mergeCascadeDescriptorSetLayoutBinding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
        mergeCascadeDescriptorSetLayoutBinding.pImmutableSamplers = nullptr;
        pipelineBuilder.AddBinding(0, mergeCascadeDescriptorSetLayoutBinding);
        mergeCascadeDescriptorSetLayoutBinding.binding = 1;
        pipelineBuilder.AddBinding(0, mergeCascadeDescriptorSetLayoutBinding);

        pipelineBuilder.SetPipelineType(Pipeline::COMPUTE);
        pipelineBuilder.SetPushConstantSize<MergeCascadesPushConstant>(VK_SHADER_STAGE_COMPUTE_BIT);

        for (int i = 0; i < MAX_LEVEL; i++) {
            mergeCascadesPipelines.push_back(pipelineBuilder.Build());
        }

        pipelineBuilder.Reset();

        pipelineBuilder.AddShaderStage(BuildGITexture, sizeof(BuildGITexture), VK_SHADER_STAGE_COMPUTE_BIT);

        VkDescriptorSetLayoutBinding buildGITextureDescriptorSetLayoutBinding{};
        buildGITextureDescriptorSetLayoutBinding.binding = 0;
        buildGITextureDescriptorSetLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        buildGITextureDescriptorSetLayoutBinding.descriptorCount = 1;
        buildGITextureDescriptorSetLayoutBinding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
        buildGITextureDescriptorSetLayoutBinding.pImmutableSamplers = nullptr;
        pipelineBuilder.AddBinding(0, buildGITextureDescriptorSetLayoutBinding);
        buildGITextureDescriptorSetLayoutBinding.binding = 1;
        pipelineBuilder.AddBinding(0, buildGITextureDescriptorSetLayoutBinding);

        pipelineBuilder.SetPipelineType(Pipeline::COMPUTE);

        buildGITexturePipeline = pipelineBuilder.Build();

        ApplySettings();
    }

    void ComputeQueueInitCommands(VkCommandBuffer cmd) override {
        TransitionImage(cmd, sdfImage.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
        TransitionImage(cmd, displayImage.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);

        fillTextureFloat4Pipeline.Bind(cmd, VK_PIPELINE_BIND_POINT_COMPUTE);
        FillTextureFloat4PushConstant pushConstant{};
        // pushConstant.width = WINDOW_WIDTH;
        // pushConstant.height = WINDOW_HEIGHT;
        pushConstant.r = 0.0f;
        pushConstant.g = 0.0f;
        pushConstant.b = 0.0f;
        pushConstant.a = 1000000.0f;

        fillTextureFloat4Pipeline.SetPushConstant(cmd, VK_SHADER_STAGE_COMPUTE_BIT, &pushConstant);

        fillTextureFloat4Pipeline.Dispatch(cmd, WINDOW_WIDTH / 8, WINDOW_HEIGHT / 8, 1);
    }

    void Update(uint32_t frame) override {
        if (!ImGui::GetIO().WantCaptureMouse) {
            isLeftMouseButtonPressed = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
        } else {
            isLeftMouseButtonPressed = false;
        }

        // Update stuff here
        ImGui::Begin("Radiance Cascade GI Settings");
        ImGui::Text("Max level");
        ImGui::InputInt("##maxlvi", (int *) &newRadianceCascadeSettings.maxLevel);
        ImGui::SliderInt("##maxlv", (int *) &newRadianceCascadeSettings.maxLevel, 1, 10);

        ImGui::Text("Vertical probe count (max level)");
        ImGui::InputInt("##vprobeci", (int *) &newRadianceCascadeSettings.verticalProbeCountAtMaxLevel);
        ImGui::SliderInt("##vprobec", (int *) &newRadianceCascadeSettings.verticalProbeCountAtMaxLevel, 1, MAX_LEVEL);

        ImGui::Text("First level radius");
        ImGui::InputFloat("##firstradii", &newRadianceCascadeSettings.radius);
        ImGui::SliderFloat("##firstradi", &newRadianceCascadeSettings.radius, .001f, .5f);

        ImGui::Text("Radius multiplier");
        ImGui::InputFloat("##radmuli", &newRadianceCascadeSettings.radiusMultiplier);
        ImGui::SliderFloat("##radmul", &newRadianceCascadeSettings.radiusMultiplier, .1f, 10.0f);

        ImGui::Text("Raymarch step size");
        ImGui::InputFloat("##raymarchstepi", &newRadianceCascadeSettings.raymarchStepSize);
        ImGui::SliderFloat("##raymarchstep", &newRadianceCascadeSettings.raymarchStepSize, .001f, .1f);

        ImGui::Text("Attenuation");
        ImGui::InputFloat("##attei", &newRadianceCascadeSettings.attenuation);
        ImGui::SliderFloat("##atte", &newRadianceCascadeSettings.attenuation, .1f, 100.0f);

        if (ImGui::Button("Apply settings")) {
            ApplySettings();
        }
        ImGui::End();

        ImGui::Begin("Pen settings");
        ImGui::ColorPicker3("Pen color", (float *) &color);
        ImGui::SliderInt("Radius", &radius, 1, 256);
        resetSDF = ImGui::Button("Reset SDF");
        ImGui::End();
    }

    void ApplySettings() {
        vkDeviceWaitIdle(device);
        for (auto &raymarchImage: raymarchImages) {
            if (raymarchImage.Initialized()) {
                DestroyImage(device, allocator, raymarchImage);
            }
        }
        if (globalIlluminationImage.Initialized()) {
            DestroyImage(device, allocator, globalIlluminationImage);
        }
        raymarchImages.clear();

        radianceCascadeSettings = newRadianceCascadeSettings;

        // size of cascades
        // first we need to know the resolution the max level cascade
        uint32_t maxLevelCascadeProbeResolution = 1 << radianceCascadeSettings.maxLevel;
        uint32_t aspectRatio = std::ceil((float) WINDOW_WIDTH / WINDOW_HEIGHT);
        uint32_t horizontalProbeCountAtMaxLevel = aspectRatio * radianceCascadeSettings.verticalProbeCountAtMaxLevel;
        uint32_t cascadeWidth = horizontalProbeCountAtMaxLevel * maxLevelCascadeProbeResolution;
        uint32_t cascadeHeight = radianceCascadeSettings.verticalProbeCountAtMaxLevel * maxLevelCascadeProbeResolution;

        std::print("Cascade resolution: {}x{}\n", cascadeWidth, cascadeHeight);

        VkImageCreateInfo imgCreateInfo{};
        imgCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imgCreateInfo.imageType = VK_IMAGE_TYPE_2D;
        imgCreateInfo.extent.width = cascadeWidth;
        imgCreateInfo.extent.height = cascadeHeight;
        imgCreateInfo.extent.depth = 1;
        imgCreateInfo.mipLevels = 1;
        imgCreateInfo.arrayLayers = 1;
        imgCreateInfo.format = VK_FORMAT_R32G32B32A32_SFLOAT;

        imgCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        imgCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        imgCreateInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
        imgCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;

        for (int i = 0; i < radianceCascadeSettings.maxLevel; i++) {
            Image raymarchImage = CreateImage(device, imgCreateInfo, allocator);
            raymarchImages.push_back(raymarchImage);

            VkDescriptorImageInfo descriptorImageInfo{};
            descriptorImageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
            descriptorImageInfo.imageView = raymarchImage.view;
            descriptorImageInfo.sampler = VK_NULL_HANDLE;
            raymarchPipelines[i].WriteToDescriptorSet(0, 1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, &descriptorImageInfo,
                                                      nullptr);
        }

        raymarchImageLayout = VK_IMAGE_LAYOUT_UNDEFINED;

        for (int i = 0; i < radianceCascadeSettings.maxLevel - 1; i++) {
            VkDescriptorImageInfo descriptorImageInfoInput{};
            descriptorImageInfoInput.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
            descriptorImageInfoInput.imageView = raymarchImages[i + 1].view;
            descriptorImageInfoInput.sampler = VK_NULL_HANDLE;
            VkDescriptorImageInfo descriptorImageInfoOutput{};
            descriptorImageInfoOutput.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
            descriptorImageInfoOutput.imageView = raymarchImages[i].view;
            descriptorImageInfoOutput.sampler = VK_NULL_HANDLE;
            mergeCascadesPipelines[i].WriteToDescriptorSet(0, 0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                                                           &descriptorImageInfoInput, nullptr);
            mergeCascadesPipelines[i].WriteToDescriptorSet(0, 1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                                                           &descriptorImageInfoOutput, nullptr);
        }

        VkImageCreateInfo imgCreateInfoOutputGI{};
        imgCreateInfoOutputGI.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imgCreateInfoOutputGI.imageType = VK_IMAGE_TYPE_2D;
        imgCreateInfoOutputGI.extent.width = cascadeWidth / 2;
        imgCreateInfoOutputGI.extent.height = cascadeHeight / 2;
        imgCreateInfoOutputGI.extent.depth = 1;
        imgCreateInfoOutputGI.mipLevels = 1;
        imgCreateInfoOutputGI.arrayLayers = 1;
        imgCreateInfoOutputGI.format = VK_FORMAT_R32G32B32A32_SFLOAT;

        imgCreateInfoOutputGI.tiling = VK_IMAGE_TILING_OPTIMAL;
        imgCreateInfoOutputGI.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        imgCreateInfoOutputGI.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT |
                                      VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
        imgCreateInfoOutputGI.samples = VK_SAMPLE_COUNT_1_BIT;

        globalIlluminationImage = CreateImage(device, imgCreateInfoOutputGI, allocator);

        outputGIImageLayout = VK_IMAGE_LAYOUT_UNDEFINED;

        VkDescriptorImageInfo descriptorImageInfoOutputGI{};
        descriptorImageInfoOutputGI.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
        descriptorImageInfoOutputGI.imageView = globalIlluminationImage.view;
        descriptorImageInfoOutputGI.sampler = VK_NULL_HANDLE;
        buildGITexturePipeline.WriteToDescriptorSet(0, 1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                                                    &descriptorImageInfoOutputGI, nullptr);

        VkDescriptorImageInfo descriptorImageInfoInputCascade{};
        descriptorImageInfoInputCascade.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
        descriptorImageInfoInputCascade.imageView = raymarchImages[0].view;
        descriptorImageInfoInputCascade.sampler = VK_NULL_HANDLE;
        buildGITexturePipeline.WriteToDescriptorSet(0, 0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                                                    &descriptorImageInfoInputCascade, nullptr);

        VkDescriptorImageInfo descriptorImageInfoInputGI{};
        descriptorImageInfoInputGI.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
        descriptorImageInfoInputGI.imageView = globalIlluminationImage.view;
        descriptorImageInfoInputGI.sampler = linearSampler;
        finalPassPipeline.WriteToDescriptorSet(0, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                               &descriptorImageInfoInputGI, nullptr);
    }

    void ComputeQueueCommands(VkCommandBuffer cmd, VkImage swapchainImage, VkImageView swapchainImageView,
                              VkExtent2D swapchainExtent) override {
        double xpos, ypos;
        glfwGetCursorPos(window, &xpos, &ypos);

        // Update push constants
        ComputeDrawToSDFTexturePushConstant pushConstant{};
        pushConstant.mousePosX = isLeftMouseButtonPressed ? xpos : -1;
        pushConstant.mousePosY = isLeftMouseButtonPressed ? ypos : -1;
        pushConstant.radius = std::clamp(radius, 0, 255);
        pushConstant.r = std::clamp((int) (255 * color[0]), 0, 255);
        pushConstant.g = std::clamp((int) (255 * color[1]), 0, 255);
        pushConstant.b = std::clamp((int) (255 * color[2]), 0, 255);

        drawToSDFTexturePipeline.Bind(cmd, VK_PIPELINE_BIND_POINT_COMPUTE);

        drawToSDFTexturePipeline.SetPushConstant(cmd, VK_SHADER_STAGE_COMPUTE_BIT, &pushConstant);

        drawToSDFTexturePipeline.Dispatch(cmd, WINDOW_WIDTH / 8, WINDOW_HEIGHT / 8, 1);

        if (resetSDF) {
            CmdWaitForPipelineStage(cmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);

            fillTextureFloat4Pipeline.Bind(cmd, VK_PIPELINE_BIND_POINT_COMPUTE);
            FillTextureFloat4PushConstant fillTextureFloat4PushConstant{};
            fillTextureFloat4PushConstant.r = 0.0f;
            fillTextureFloat4PushConstant.g = 0.0f;
            fillTextureFloat4PushConstant.b = 0.0f;
            fillTextureFloat4PushConstant.a = 1000000.0f;

            fillTextureFloat4Pipeline.SetPushConstant(cmd, VK_SHADER_STAGE_COMPUTE_BIT, &fillTextureFloat4PushConstant);

            fillTextureFloat4Pipeline.Dispatch(cmd, WINDOW_WIDTH / 8, WINDOW_HEIGHT / 8, 1);
        }

        if (raymarchImageLayout != VK_IMAGE_LAYOUT_GENERAL) {
            for (auto &raymarchImage: raymarchImages) {
                TransitionImage(cmd, raymarchImage.image, raymarchImageLayout, VK_IMAGE_LAYOUT_GENERAL);
            }
            raymarchImageLayout = VK_IMAGE_LAYOUT_GENERAL;
        }

        if (outputGIImageLayout != VK_IMAGE_LAYOUT_GENERAL) {
            TransitionImage(cmd, globalIlluminationImage.image, outputGIImageLayout, VK_IMAGE_LAYOUT_GENERAL);
            outputGIImageLayout = VK_IMAGE_LAYOUT_GENERAL;
        }

        // Raymarch
        RaymarchPushConstant raymarchPushConstant{};
        raymarchPushConstant.radianceCascadeSettings = radianceCascadeSettings;

        // size of cascades
        // first we need to know the resolution the max level cascade
        uint32_t maxLevelCascadeProbeResolution = 1 << radianceCascadeSettings.maxLevel;
        uint32_t aspectRatio = std::ceil((float) WINDOW_WIDTH / WINDOW_HEIGHT);
        uint32_t horizontalProbeCountAtMaxLevel = aspectRatio * radianceCascadeSettings.verticalProbeCountAtMaxLevel;
        uint32_t cascadeWidth = horizontalProbeCountAtMaxLevel * maxLevelCascadeProbeResolution;
        uint32_t cascadeHeight = radianceCascadeSettings.verticalProbeCountAtMaxLevel * maxLevelCascadeProbeResolution;

        for (int i = 0; i < radianceCascadeSettings.maxLevel; i++) {
            // CmdWaitForPipelineStage(cmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT); // No need, can be done in parallel
            raymarchPushConstant.currentLevel = i;
            raymarchPipelines[i].Bind(cmd, VK_PIPELINE_BIND_POINT_COMPUTE);
            raymarchPipelines[i].SetPushConstant(cmd, VK_SHADER_STAGE_COMPUTE_BIT, &raymarchPushConstant);
            raymarchPipelines[i].Dispatch(cmd, cascadeWidth / 8, cascadeHeight / 8, 1);
        }

        MergeCascadesPushConstant mergeCascadesPushConstant{};
        mergeCascadesPushConstant.radianceCascadeSettings = radianceCascadeSettings;

        for (int i = radianceCascadeSettings.maxLevel - 2; i >= 0; i--) {
            CmdWaitForPipelineStage(cmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
            mergeCascadesPushConstant.outputLevel = i;
            mergeCascadesPipelines[i].Bind(cmd, VK_PIPELINE_BIND_POINT_COMPUTE);
            mergeCascadesPipelines[i].SetPushConstant(cmd, VK_SHADER_STAGE_COMPUTE_BIT, &mergeCascadesPushConstant);
            mergeCascadesPipelines[i].Dispatch(cmd, cascadeWidth / 8, cascadeHeight / 8, 1);
        }

        CmdWaitForPipelineStage(cmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);
        buildGITexturePipeline.Bind(cmd, VK_PIPELINE_BIND_POINT_COMPUTE);

        buildGITexturePipeline.Dispatch(cmd, cascadeWidth / 16, cascadeHeight / 16, 1);

        CmdWaitForPipelineStage(cmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);

        finalPassPipeline.Bind(cmd, VK_PIPELINE_BIND_POINT_COMPUTE);

        finalPassPipeline.Dispatch(cmd, WINDOW_WIDTH / 8, WINDOW_HEIGHT / 8, 1);
    }

    void GraphicsQueueCommands(VkCommandBuffer cmd, VkImage swapchainImage, VkImageView swapchainImageView,
                               VkExtent2D swapchainExtent) override {
        // size of cascades
        // first we need to know the resolution the max level cascade
        // uint32_t maxLevelCascadeProbeResolution = 1 << radianceCascadeSettings.maxLevel;
        // uint32_t aspectRatio = std::ceil((float) WINDOW_WIDTH / WINDOW_HEIGHT);
        // uint32_t horizontalProbeCountAtMaxLevel = aspectRatio * radianceCascadeSettings.verticalProbeCountAtMaxLevel;
        // uint32_t cascadeWidth = horizontalProbeCountAtMaxLevel * maxLevelCascadeProbeResolution;
        // uint32_t cascadeHeight = radianceCascadeSettings.verticalProbeCountAtMaxLevel * maxLevelCascadeProbeResolution;

        VkImageBlit region{};
        region.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        region.dstSubresource.layerCount = 1;
        region.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        region.srcSubresource.layerCount = 1;
        region.srcOffsets[1].x = WINDOW_WIDTH;
        region.srcOffsets[1].y = WINDOW_HEIGHT;
        region.srcOffsets[1].z = 1;
        region.dstOffsets[1].x = swapchainExtent.width;
        region.dstOffsets[1].y = swapchainExtent.height;
        region.dstOffsets[1].z = 1;

        vkCmdBlitImage(cmd, displayImage.image, VK_IMAGE_LAYOUT_GENERAL, swapchainImage, VK_IMAGE_LAYOUT_GENERAL, 1,
                       &region, VK_FILTER_LINEAR);
    }

    void Cleanup() override {
        fillTextureFloat4Pipeline.Destroy();
        drawToSDFTexturePipeline.Destroy();
        finalPassPipeline.Destroy();
        for (auto &raymarchPipeline: raymarchPipelines) {
            raymarchPipeline.Destroy();
        }
        for (auto &mergeCascadesPipeline: mergeCascadesPipelines) {
            mergeCascadesPipeline.Destroy();
        }
        buildGITexturePipeline.Destroy();
        // ImGui_ImplVulkan_RemoveTexture(imguiImageDescriptorSet);
        // vkDestroySampler(device, imguiSampler, nullptr);
        for (auto &raymarchImage: raymarchImages) {
            if (raymarchImage.Initialized()) {
                DestroyImage(device, allocator, raymarchImage);
            }
        }
        if (globalIlluminationImage.Initialized()) {
            DestroyImage(device, allocator, globalIlluminationImage);
        }
        vkDestroySampler(device, linearSampler, nullptr);
        DestroyImage(device, allocator, displayImage);
        DestroyImage(device, allocator, sdfImage);
    }

private:
    Image sdfImage{};
    Image displayImage{};
    std::vector<Image> raymarchImages{};
    Image globalIlluminationImage{};
    VkSampler linearSampler{};
    VkImageLayout raymarchImageLayout;
    VkImageLayout outputGIImageLayout;
    VkSampler imguiSampler{};
    VkDescriptorSet imguiImageDescriptorSet{};
    Pipeline drawToSDFTexturePipeline{};
    Pipeline fillTextureFloat4Pipeline{};
    Pipeline finalPassPipeline{};
    std::vector<Pipeline> raymarchPipelines{};
    std::vector<Pipeline> mergeCascadesPipelines{};
    Pipeline buildGITexturePipeline{};
    bool isLeftMouseButtonPressed = false;
    bool resetSDF = false;

    RadianceCascadeSettings radianceCascadeSettings{
        .maxLevel = 8,
        .verticalProbeCountAtMaxLevel = 4,
        .radius = .01f,
        .radiusMultiplier = 1.5f,
        .raymarchStepSize = 0.01f,
        .attenuation = 100.f
    };
    RadianceCascadeSettings newRadianceCascadeSettings{
        .maxLevel = 8,
        .verticalProbeCountAtMaxLevel = 4,
        .radius = .01f,
        .radiusMultiplier = 1.5f,
        .raymarchStepSize = 0.01f,
        .attenuation = 100.f
    };

    // draw settings
    int radius = 10;
    float color[3] = {1.0f, 0.0f, 0.0f};
};

REGISTER_COMPUTE_APP(ComputeAppImpl)
