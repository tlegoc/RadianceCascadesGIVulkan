//
// Created by theo on 06/10/2024.
//

#pragma once
#include <memory>
#include <vector>
#include <unordered_map>
#include <vulkan/vulkan_core.h>

class Pipeline {
public:
    Pipeline();

    void Destroy();

    enum PipelineType {
        GRAPHICS,
        COMPUTE
    };

    void Bind(VkCommandBuffer cmd, VkPipelineBindPoint bindPoint);

    void WriteToDescriptorSet(uint32_t set, uint32_t binding, VkDescriptorType type, VkDescriptorImageInfo *imageInfo,
                              VkDescriptorBufferInfo *bufferInfo);

    template<typename T>
    void SetPushConstant(VkCommandBuffer cmd, VkShaderStageFlags stage, T* data) {
        SetPushConstant(cmd, stage, data, sizeof(T));
    }

    void SetPushConstant(VkCommandBuffer cmd, VkShaderStageFlags stage, const void *data, size_t size);

    void Dispatch(VkCommandBuffer cmd, uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ);


    Pipeline(PipelineType type, VkDevice device, VkPipeline pipeline, VkPipelineLayout layout,
             std::unordered_map<uint32_t, VkDescriptorSet> descriptorSets,
             std::unordered_map<uint32_t, VkDescriptorSetLayout> descriptorSetLayouts, VkDescriptorPool descriptorPool);
private:
    bool m_valid = false;
    PipelineType m_type{};
    VkDevice m_device{};
    VkPipeline m_pipeline{};
    VkPipelineLayout m_layout{};
    std::unordered_map<uint32_t, VkDescriptorSetLayout> m_descriptorSetLayouts{};
    std::unordered_map<uint32_t, VkDescriptorSet> m_descriptorSets{};
    VkDescriptorPool m_descriptorPool{};
};

class PipelineBuilder {
public:
    explicit PipelineBuilder(VkDevice device);

    ~PipelineBuilder() {
        Reset();
    }

    void AddBinding(int set, VkDescriptorSetLayoutBinding binding);

    void AddShaderStage(VkShaderModule shader, VkShaderStageFlagBits stage, VkPipelineShaderStageCreateFlags flags = 0);

    void AddShaderStage(const uint32_t *code, size_t size, VkShaderStageFlagBits stage,
                        VkPipelineShaderStageCreateFlags flags = 0);

    void SetPipelineType(Pipeline::PipelineType type);

    template<typename T>
    void SetPushConstantSize(VkShaderStageFlags stage) {
        SetPushConstantSize(stage, sizeof(T));
    }

    void SetPushConstantSize(VkShaderStageFlags stage, size_t size);

    Pipeline Build();

    void Reset();

private:
    Pipeline::PipelineType m_type{};
    VkDevice m_device;
    std::unordered_map<uint32_t, std::vector<VkDescriptorSetLayoutBinding> > m_bindings;
    std::unordered_map<VkShaderStageFlagBits, VkPipelineShaderStageCreateInfo> m_stages;
    std::vector<VkShaderModule> m_shaderModules;
    std::unordered_map<VkShaderStageFlags, VkPushConstantRange> m_ranges;
};
