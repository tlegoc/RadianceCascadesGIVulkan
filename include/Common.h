//
// Created by theo on 02/10/2024.
//

#pragma once

#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>
#include <vulkan/vk_enum_string_helper.h>

#include <print>

#define VK_CHECK(x)                                                     \
do {                                                                \
VkResult err = x;                                               \
if (err) {                                                      \
std::print("Detected Vulkan error: {}", string_VkResult(err)); \
throw std::runtime_error(std::format("Detected Vulkan error: {}", string_VkResult(err)));                                                    \
}                                                               \
} while (0)

void TransitionImage(VkCommandBuffer cmd, VkImage image, VkImageLayout oldLayout, VkImageLayout newLayout);

struct Image {
    VkImage image;
    VkImageView view;
    VmaAllocation memory;
};

Image CreateImage(VkDevice device, VkImageCreateInfo imgCreateInfo, VmaAllocator allocator);

void DestroyImage(VkDevice device, VmaAllocator allocator, Image img);

VkShaderModule CreateShaderModule(VkDevice device, const uint32_t* code, size_t size);