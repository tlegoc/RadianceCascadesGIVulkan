#include <iostream>

#include <VkBootstrap.h>

#include "GLFW/glfw3.h"

#define WINDOW_WIDTH 800
#define WINDOW_HEIGHT 600
#define MAX_FRAMES_IN_FLIGHT 2

int main() {
    // Window creation
    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
    GLFWwindow *window = glfwCreateWindow(WINDOW_WIDTH, WINDOW_HEIGHT, "Vulkan window", nullptr, nullptr);

    vkb::InstanceBuilder instanceBuilder;
    auto instanceBuilderResult = instanceBuilder.set_app_name("Example Vulkan Application")
            .enable_validation_layers()
            .require_api_version(1, 3, 0)
            .use_default_debug_messenger()
            .build();

    if (!instanceBuilderResult) {
        std::cerr << "Failed to create Vulkan instance. Error: " << instanceBuilderResult.error().message() << "\n";
        return false;
    }

    vkb::Instance instance = instanceBuilderResult.value();

    VkSurfaceKHR surface;
    if (glfwCreateWindowSurface(instance.instance, window, nullptr, &surface) != VK_SUCCESS) {
        std::cerr << "Failed to create window surface\n";
        return false;
    }

    vkb::PhysicalDeviceSelector physicalDeviceSelector{instance};
    VkPhysicalDeviceVulkan13Features vulkan13Features{};
    vulkan13Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
    vulkan13Features.synchronization2 = VK_TRUE;

    auto physicalDeviceSelectorResult = physicalDeviceSelector.set_surface(surface)
            .set_minimum_version(1, 3)
            .add_required_extension_features(vulkan13Features)
            .select();

    if (!physicalDeviceSelectorResult) {
        std::cerr << "Failed to select Vulkan Physical Device. Error: " << physicalDeviceSelectorResult.error().message() << "\n";
        return false;
    }

    vkb::DeviceBuilder deviceBuilder{physicalDeviceSelectorResult.value()};
    // automatically propagate needed data from instance & physical device
    auto deviceBuilderResult = deviceBuilder.build();
    if (!deviceBuilderResult) {
        std::cerr << "Failed to create Vulkan device. Error: " << deviceBuilderResult.error().message() << "\n";
        return false;
    }
    vkb::Device device = deviceBuilderResult.value();

    // Get the VkDevice handle used in the rest of a vulkan application

    // Get the graphics queue with a helper function
    // We will use it only for presenting
    auto graphicsQueueResult = device.get_queue(vkb::QueueType::graphics);
    if (!graphicsQueueResult) {
        std::cerr << "Failed to get graphics queue. Error: " << graphicsQueueResult.error().message() << "\n";
        return false;
    }
    VkQueue graphicsQueue = graphicsQueueResult.value();

    // Get the compute queue with a helper function
    auto computeQueueResult = device.get_queue(vkb::QueueType::compute);
    if (!computeQueueResult) {
        std::cerr << "Failed to get graphics queue. Error: " << graphicsQueueResult.error().message() << "\n";
        return false;
    }
    VkQueue computeQueue = computeQueueResult.value();

    vkb::SwapchainBuilder swapchainBuilder{device};
    auto swapchainBuilderResult = swapchainBuilder.use_default_format_selection()
            .set_desired_present_mode(VK_PRESENT_MODE_FIFO_KHR)
            .set_desired_extent(WINDOW_WIDTH, WINDOW_HEIGHT)
            .build();

    vkb::Swapchain swapchain = swapchainBuilderResult.value();

    auto imageViews = swapchainBuilderResult->get_image_views().value();
    auto images = swapchain.get_images().value();

    // Synchronization
    std::vector<VkSemaphore> imageAvailableSemaphores;
    std::vector<VkSemaphore> renderFinishedSemaphores;
    std::vector<VkFence> inFlightFences;

    imageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
    renderFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
    inFlightFences.resize(MAX_FRAMES_IN_FLIGHT);

    VkSemaphoreCreateInfo semaphoreInfo{};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        if (vkCreateSemaphore(device, &semaphoreInfo, nullptr, &imageAvailableSemaphores[i]) != VK_SUCCESS ||
            vkCreateSemaphore(device, &semaphoreInfo, nullptr, &renderFinishedSemaphores[i]) != VK_SUCCESS ||
            vkCreateFence(device, &fenceInfo, nullptr, &inFlightFences[i]) != VK_SUCCESS) {
            throw std::runtime_error("failed to create synchronization objects for a frame!");
        }
    }

    // Command Pools
    VkCommandPool computeCommandPool;
    VkCommandPoolCreateInfo computeCommandPoolCreateInfo{};
    computeCommandPoolCreateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    computeCommandPoolCreateInfo.queueFamilyIndex = device.get_queue_index(vkb::QueueType::compute).value();
    computeCommandPoolCreateInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

    if (vkCreateCommandPool(device, &computeCommandPoolCreateInfo, nullptr, &computeCommandPool) != VK_SUCCESS) {
        throw std::runtime_error("failed to create command pool!");
    }

    std::vector<VkCommandBuffer> commandBuffers;
    commandBuffers.resize(MAX_FRAMES_IN_FLIGHT);
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = computeCommandPool;
    allocInfo.commandBufferCount = static_cast<uint32_t>(commandBuffers.size());

    if (vkAllocateCommandBuffers(device, &allocInfo, commandBuffers.data()) != VK_SUCCESS) {
        throw std::runtime_error("failed to allocate command buffers!");
    }

    uint32_t currentFrame = 0;
    uint32_t imageIndex = 0;

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        vkWaitForFences(device, 1, &inFlightFences[currentFrame], VK_TRUE, UINT64_MAX);
        vkResetFences(device, 1, &inFlightFences[currentFrame]);

        vkAcquireNextImageKHR(device, swapchain, UINT64_MAX, imageAvailableSemaphores[currentFrame], VK_NULL_HANDLE,
                              &imageIndex);

        vkResetCommandBuffer(commandBuffers[currentFrame], 0);

        VkCommandBuffer cmd = commandBuffers[currentFrame];

        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

        vkBeginCommandBuffer(cmd, &beginInfo);
        // RECORD COMPUTE COMMANDS HERE

        // STOP

        // Transition image layout
        VkImageMemoryBarrier2 imageBarrier{.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2};
        imageBarrier.pNext = nullptr;
        imageBarrier.srcStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
        imageBarrier.srcAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT;
        imageBarrier.dstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
        imageBarrier.dstAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT | VK_ACCESS_2_MEMORY_READ_BIT;
        imageBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        imageBarrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

        VkImageSubresourceRange subImage{};
        VkImageAspectFlags aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        subImage.aspectMask = aspectMask;
        subImage.baseMipLevel = 0;
        subImage.levelCount = VK_REMAINING_MIP_LEVELS;
        subImage.baseArrayLayer = 0;
        subImage.layerCount = VK_REMAINING_ARRAY_LAYERS;

        imageBarrier.subresourceRange = subImage;
        imageBarrier.image = images[imageIndex];

        VkDependencyInfo depInfo{};
        depInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
        depInfo.pNext = nullptr;

        depInfo.imageMemoryBarrierCount = 1;
        depInfo.pImageMemoryBarriers = &imageBarrier;

        vkCmdPipelineBarrier2(cmd, &depInfo);
        vkEndCommandBuffer(cmd);

        VkSemaphore waitSemaphores[] = {imageAvailableSemaphores[currentFrame]};
        VkSemaphore signalSemaphores[] = {renderFinishedSemaphores[currentFrame]};
        VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_ALL_COMMANDS_BIT};

        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &cmd;
        submitInfo.waitSemaphoreCount = 1;
        submitInfo.pWaitSemaphores = waitSemaphores;
        submitInfo.pWaitDstStageMask = waitStages;
        submitInfo.signalSemaphoreCount = 1;
        submitInfo.pSignalSemaphores = signalSemaphores;

        if (vkQueueSubmit(computeQueue, 1, &submitInfo, inFlightFences[currentFrame]) != VK_SUCCESS) {
            throw std::runtime_error("Error submitting command buffer!");
        }

        VkPresentInfoKHR presentInfo{};
        presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

        presentInfo.waitSemaphoreCount = 1;
        presentInfo.pWaitSemaphores = signalSemaphores;

        VkSwapchainKHR swapChains[] = {swapchain};
        presentInfo.swapchainCount = 1;
        presentInfo.pSwapchains = swapChains;
        presentInfo.pImageIndices = &imageIndex;

        vkQueuePresentKHR(graphicsQueue, &presentInfo);

        currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
    }

    vkDeviceWaitIdle(device);

    vkDestroyCommandPool(device, computeCommandPool, nullptr);

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        vkDestroySemaphore(device, renderFinishedSemaphores[i], nullptr);
        vkDestroySemaphore(device, imageAvailableSemaphores[i], nullptr);
        vkDestroyFence(device, inFlightFences[i], nullptr);
    }

    for (auto &image_view: imageViews) {
        vkDestroyImageView(device, image_view, nullptr);
    }
    destroy_swapchain(swapchain);
    destroy_device(device);
    destroy_surface(instance, surface);
    destroy_instance(instance);

    return 0;
}
