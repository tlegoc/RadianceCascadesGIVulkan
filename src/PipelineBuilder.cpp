//
// Created by theo on 06/10/2024.
//

#include <Common.h>
#include <PipelineBuilder.h>

#include <utility>

Pipeline::Pipeline() = default;

void Pipeline::Bind(VkCommandBuffer cmd, VkPipelineBindPoint bindPoint) {
    if (!m_valid) {
        throw std::runtime_error("Pipeline not valid");
    }
    vkCmdBindPipeline(cmd, bindPoint, m_pipeline);

    for (auto &[key, descriptorSet]: m_descriptorSets) {
        vkCmdBindDescriptorSets(cmd, bindPoint, m_layout, key, 1, &descriptorSet, 0, nullptr);
    }
}

void Pipeline::WriteToDescriptorSet(uint32_t set, uint32_t binding, VkDescriptorType type,
                                    VkDescriptorImageInfo *imageInfo, VkDescriptorBufferInfo *bufferInfo) {
    if (!m_valid) {
        throw std::runtime_error("Pipeline not valid");
    }

    VkWriteDescriptorSet writeDescriptorSet{};
    writeDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writeDescriptorSet.dstSet = m_descriptorSets[set];
    writeDescriptorSet.dstBinding = binding;
    writeDescriptorSet.descriptorType = type;
    writeDescriptorSet.descriptorCount = 1;
    writeDescriptorSet.pImageInfo = imageInfo;
    writeDescriptorSet.pBufferInfo = bufferInfo;

    vkUpdateDescriptorSets(m_device, 1, &writeDescriptorSet, 0, nullptr);
}

void Pipeline::SetPushConstant(VkCommandBuffer cmd, VkShaderStageFlags stage, const void *data, size_t size) {
    if (!m_valid) {
        throw std::runtime_error("Pipeline not valid");
    }
    vkCmdPushConstants(cmd, m_layout, stage, 0, size, data);
}

void Pipeline::Dispatch(VkCommandBuffer cmd, uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ) {
    if (!m_valid) {
        throw std::runtime_error("Pipeline not valid");
    }
    vkCmdDispatch(cmd, groupCountX, groupCountY, groupCountZ);
}

Pipeline::Pipeline(PipelineType type, VkDevice device, VkPipeline pipeline, VkPipelineLayout layout,
                   std::unordered_map<uint32_t, VkDescriptorSet> descriptorSets,
                   std::unordered_map<uint32_t, VkDescriptorSetLayout> descriptorSetLayouts,
                   VkDescriptorPool descriptorPool) {
    m_type = type;
    m_device = device;
    m_pipeline = pipeline;
    m_layout = layout;
    m_descriptorSets = std::move(descriptorSets);
    m_descriptorPool = descriptorPool;
    m_descriptorSetLayouts = std::move(descriptorSetLayouts);
    m_valid = true;
}

void Pipeline::Destroy() {
    if (!m_valid) {
        throw std::runtime_error("Pipeline not valid");
    }
    for (auto &[key, layout]: m_descriptorSetLayouts) {
        vkDestroyDescriptorSetLayout(m_device, layout, nullptr);
    }
    vkDestroyPipeline(m_device, m_pipeline, nullptr);
    vkDestroyPipelineLayout(m_device, m_layout, nullptr);
    vkDestroyDescriptorPool(m_device, m_descriptorPool, nullptr);
}

PipelineBuilder::PipelineBuilder(VkDevice device) : m_device(device) {
}

void PipelineBuilder::AddBinding(int set, VkDescriptorSetLayoutBinding binding) {
    if (m_bindings.find(set) == m_bindings.end()) {
        m_bindings[set] = {};
    }
    m_bindings[set].emplace_back(binding);
}

void PipelineBuilder::AddShaderStage(VkShaderModule shader, VkShaderStageFlagBits stage,
                                     VkPipelineShaderStageCreateFlags flags) {
    VkPipelineShaderStageCreateInfo stageCreateInfo{};
    stageCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stageCreateInfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    stageCreateInfo.module = shader;
    stageCreateInfo.flags = flags;
    stageCreateInfo.pName = "main";

    m_stages[stage] = stageCreateInfo;
}

void PipelineBuilder::AddShaderStage(const uint32_t *code, size_t size, VkShaderStageFlagBits stage,
                                     VkPipelineShaderStageCreateFlags flags) {
    VkShaderModule mod = CreateShaderModule(m_device, code, size);
    AddShaderStage(mod, stage, flags);
    m_shaderModules.push_back(mod);
}

void PipelineBuilder::SetPipelineType(Pipeline::PipelineType type) {
    m_type = type;
}

void PipelineBuilder::SetPushConstantSize(VkShaderStageFlags stage, size_t size) {
    VkPushConstantRange range{};
    range.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    range.offset = 0;
    range.size = size;

    m_ranges[stage] = range;
}

Pipeline PipelineBuilder::Build() {
    // Crete descriptor set layout
    std::unordered_map<uint32_t, VkDescriptorSetLayout> descriptorSetLayouts;
    std::vector<VkDescriptorPoolSize> poolSizes;
    for (auto &[key, bindings]: m_bindings) {
        VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo{};
        descriptorSetLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        descriptorSetLayoutCreateInfo.bindingCount = bindings.size();
        descriptorSetLayoutCreateInfo.pBindings = bindings.data();
        descriptorSetLayoutCreateInfo.flags = 0;

        VkDescriptorSetLayout descriptorSetLayout;
        vkCreateDescriptorSetLayout(m_device, &descriptorSetLayoutCreateInfo, nullptr, &descriptorSetLayout);

        for (auto &binding: bindings) {
            VkDescriptorPoolSize poolSize{};
            poolSize.type = binding.descriptorType;
            poolSize.descriptorCount = bindings.size();
            poolSizes.push_back(poolSize);
        }

        descriptorSetLayouts[key] = descriptorSetLayout;
    }

    // Create descriptor pool
    VkDescriptorPoolCreateInfo poolCreateInfo{};
    poolCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolCreateInfo.poolSizeCount = poolSizes.size();
    poolCreateInfo.pPoolSizes = poolSizes.data();
    poolCreateInfo.maxSets = 1;

    VkDescriptorPool pool;
    vkCreateDescriptorPool(m_device, &poolCreateInfo, nullptr, &pool);

    // Allocate descriptor set
    std::unordered_map<uint32_t, VkDescriptorSet> descriptorSets;
    for (auto &[key, layout]: descriptorSetLayouts) {
        VkDescriptorSetAllocateInfo allocateInfo{};
        allocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocateInfo.descriptorPool = pool;
        allocateInfo.descriptorSetCount = 1;
        allocateInfo.pSetLayouts = &layout;

        VkDescriptorSet descriptorSet;
        vkAllocateDescriptorSets(m_device, &allocateInfo, &descriptorSet);

        descriptorSets[key] = descriptorSet;
    }

    // Setup push constant and pipeline layout
    std::vector<VkPushConstantRange> ranges;
    for (auto &range: m_ranges) {
        ranges.push_back(range.second);
    }

    std::vector<VkDescriptorSetLayout> descriptorSetLayoutsVec;
    for (auto &[key, layout]: descriptorSetLayouts) {
        descriptorSetLayoutsVec.push_back(layout);
    }

    VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo{};
    pipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutCreateInfo.setLayoutCount = descriptorSetLayoutsVec.size();
    pipelineLayoutCreateInfo.pSetLayouts = descriptorSetLayoutsVec.data();
    pipelineLayoutCreateInfo.pushConstantRangeCount = ranges.size();
    pipelineLayoutCreateInfo.pPushConstantRanges = ranges.data();
    pipelineLayoutCreateInfo.flags = 0;

    VkPipelineLayout pipelineLayout;
    vkCreatePipelineLayout(m_device, &pipelineLayoutCreateInfo, nullptr, &pipelineLayout);

    VkPipeline pipeline;
    VkComputePipelineCreateInfo pipelineCreateInfo{};
    switch (m_type) {
        case Pipeline::GRAPHICS:
            throw std::runtime_error("Graphics pipeline not implemented");
            break;
        case Pipeline::COMPUTE:
            pipelineCreateInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
            pipelineCreateInfo.stage = m_stages[VK_SHADER_STAGE_COMPUTE_BIT];;
            pipelineCreateInfo.layout = pipelineLayout;
            vkCreateComputePipelines(m_device, VK_NULL_HANDLE, 1, &pipelineCreateInfo, nullptr, &pipeline);
            break;
        default:
            break;
    }

    return Pipeline(m_type, m_device, pipeline, pipelineLayout, descriptorSets, descriptorSetLayouts, pool);
}

void PipelineBuilder::Reset() {
    for (auto &shader: m_shaderModules) {
        vkDestroyShaderModule(m_device, shader, nullptr);
    }
    m_type = {};
    m_bindings = {};
    m_stages = {};
    m_shaderModules = {};
    m_ranges = {};
}
