#include <Common.h>
#include <ComputeApp.h>
#include <ComputeAppConfig.h>

#include <VkBootstrap.h>
#include <GLFW/glfw3.h>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_vulkan.h>

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
        std::print("Failed to create Vulkan instance. Error: {}\n", instanceBuilderResult.error().message());
        return 1;
    }

    vkb::Instance instance = instanceBuilderResult.value();

    VkSurfaceKHR surface;
    VK_CHECK(glfwCreateWindowSurface(instance.instance, window, nullptr, &surface));

    vkb::PhysicalDeviceSelector physicalDeviceSelector{instance};
    VkPhysicalDeviceVulkan13Features vulkan13Features{};
    vulkan13Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
    vulkan13Features.synchronization2 = VK_TRUE;
    vulkan13Features.dynamicRendering = VK_TRUE;
    VkPhysicalDeviceVulkan11Features vulkan11Features{};
    vulkan11Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES;
    vulkan11Features.storagePushConstant16 = VK_TRUE;
    VkPhysicalDeviceVulkan12Features vulkan12Features{};
    vulkan12Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
    vulkan12Features.storagePushConstant8 = VK_TRUE;
    vulkan12Features.shaderInt8 = VK_TRUE;
    vulkan12Features.shaderFloat16 = VK_TRUE;
    VkPhysicalDeviceFeatures features{};
    features.shaderInt16 = VK_TRUE;

    auto physicalDeviceSelectorResult = physicalDeviceSelector.set_surface(surface)
            .set_minimum_version(1, 3)
            .set_required_features_13(vulkan13Features)
            .set_required_features_12(vulkan12Features)
            .set_required_features_11(vulkan11Features)
            .set_required_features(features)
            .select();

    if (!physicalDeviceSelectorResult) {
        std::print("Failed to select Vulkan physical device. Error: {}\n",
                   physicalDeviceSelectorResult.error().message());
        return 1;
    }

    vkb::DeviceBuilder deviceBuilder{physicalDeviceSelectorResult.value()};
    // automatically propagate needed data from instance & physical device
    auto deviceBuilderResult = deviceBuilder.build();
    if (!deviceBuilderResult) {
        std::print("Failed to create Vulkan device. Error: {}\n", deviceBuilderResult.error().message());
        return 1;
    }
    vkb::Device device = deviceBuilderResult.value();

    // Get the VkDevice handle used in the rest of a vulkan application

    // Get the graphics queue with a helper function
    // We will use it only for presenting
    auto graphicsQueueResult = device.get_queue(vkb::QueueType::graphics);
    if (!graphicsQueueResult) {
        std::print("Failed to get graphics queue. Error: {}\n", graphicsQueueResult.error().message());
        return 1;
    }
    VkQueue graphicsQueue = graphicsQueueResult.value();

    // Get the compute queue with a helper function
    auto computeQueueResult = device.get_queue(vkb::QueueType::compute);
    if (!computeQueueResult) {
        std::print("Failed to get compute queue. Error: {}\n", computeQueueResult.error().message());
        return 1;
    }
    VkQueue computeQueue = computeQueueResult.value();

    vkb::SwapchainBuilder swapchainBuilder{device};
    auto swapchainBuilderResult = swapchainBuilder.use_default_format_selection()
            .set_desired_present_mode(VK_PRESENT_MODE_FIFO_KHR)
            .set_desired_extent(WINDOW_WIDTH, WINDOW_HEIGHT)
            .add_image_usage_flags(VK_IMAGE_USAGE_TRANSFER_DST_BIT)
            .build();

    vkb::Swapchain swapchain = swapchainBuilderResult.value();

    auto imageViews = swapchainBuilderResult->get_image_views().value();
    auto images = swapchain.get_images().value();

    // Synchronization
    std::vector<VkSemaphore> imageAvailableSemaphores;
    std::vector<VkSemaphore> computeFinishedSemaphores;
    std::vector<VkSemaphore> graphicsFinishedSemaphores;
    std::vector<VkFence> inFlightFences;

    imageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
    computeFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
    graphicsFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
    inFlightFences.resize(MAX_FRAMES_IN_FLIGHT);

    VkSemaphoreCreateInfo semaphoreInfo{};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        VK_CHECK(vkCreateSemaphore(device, &semaphoreInfo, nullptr, &imageAvailableSemaphores[i]));
        VK_CHECK(vkCreateSemaphore(device, &semaphoreInfo, nullptr, &computeFinishedSemaphores[i]));
        VK_CHECK(vkCreateSemaphore(device, &semaphoreInfo, nullptr, &graphicsFinishedSemaphores[i]));
        VK_CHECK(vkCreateFence(device, &fenceInfo, nullptr, &inFlightFences[i]));
    }

    // Compute Command Pools
    VkCommandPool computeCommandPool;
    VkCommandPoolCreateInfo computeCommandPoolCreateInfo{};
    computeCommandPoolCreateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    computeCommandPoolCreateInfo.queueFamilyIndex = device.get_queue_index(vkb::QueueType::compute).value();
    computeCommandPoolCreateInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

    VK_CHECK(vkCreateCommandPool(device, &computeCommandPoolCreateInfo, nullptr, &computeCommandPool));

    std::vector<VkCommandBuffer> computeCommandBuffers;
    computeCommandBuffers.resize(MAX_FRAMES_IN_FLIGHT);
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = computeCommandPool;
    allocInfo.commandBufferCount = static_cast<uint32_t>(computeCommandBuffers.size());

    VK_CHECK(vkAllocateCommandBuffers(device, &allocInfo, computeCommandBuffers.data()));

    // Graphics Command Pools
    VkCommandPool graphicsCommandPool;
    VkCommandPoolCreateInfo graphicsCommandPoolCreateInfo{};
    graphicsCommandPoolCreateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    graphicsCommandPoolCreateInfo.queueFamilyIndex = device.get_queue_index(vkb::QueueType::graphics).value();
    graphicsCommandPoolCreateInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

    VK_CHECK(vkCreateCommandPool(device, &graphicsCommandPoolCreateInfo, nullptr, &graphicsCommandPool));

    std::vector<VkCommandBuffer> graphicsCommandBuffers;
    graphicsCommandBuffers.resize(MAX_FRAMES_IN_FLIGHT);
    allocInfo.commandPool = graphicsCommandPool;
    allocInfo.commandBufferCount = static_cast<uint32_t>(graphicsCommandBuffers.size());

    VK_CHECK(vkAllocateCommandBuffers(device, &allocInfo, graphicsCommandBuffers.data()));

    // Imgui
    VkDescriptorPoolSize poolSizes[] =
    {
        {VK_DESCRIPTOR_TYPE_SAMPLER, 1000},
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000},
        {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000},
        {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000},
        {VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000},
        {VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000},
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000},
        {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000},
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000},
        {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000},
        {VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000}
    };

    VkDescriptorPoolCreateInfo poolInfo = {};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    poolInfo.maxSets = 1000;
    poolInfo.poolSizeCount = std::size(poolSizes);
    poolInfo.pPoolSizes = poolSizes;

    VkDescriptorPool imguiPool;
    VK_CHECK(vkCreateDescriptorPool(device, &poolInfo, nullptr, &imguiPool));

    ImGui::CreateContext();

    //this initializes imgui for SDL
    ImGui_ImplGlfw_InitForVulkan(window, true);

    //this initializes imgui for Vulkan
    ImGui_ImplVulkan_InitInfo imguiInitInfo = {};
    imguiInitInfo.Instance = instance;
    imguiInitInfo.PhysicalDevice = physicalDeviceSelectorResult.value().physical_device;
    imguiInitInfo.Device = device;
    imguiInitInfo.Queue = graphicsQueue;
    imguiInitInfo.DescriptorPool = imguiPool;
    imguiInitInfo.MinImageCount = 3;
    imguiInitInfo.ImageCount = 3;
    imguiInitInfo.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
    imguiInitInfo.UseDynamicRendering = VK_TRUE;
    VkPipelineRenderingCreateInfoKHR imguiPipelineInfo = {};
    imguiPipelineInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR;
    imguiPipelineInfo.colorAttachmentCount = 1;
    imguiPipelineInfo.pColorAttachmentFormats = &swapchain.image_format;
    imguiInitInfo.PipelineRenderingCreateInfo = imguiPipelineInfo;

    ImGui_ImplVulkan_Init(&imguiInitInfo);

    // VMA
    VmaAllocatorCreateInfo allocatorInfo{};
    allocatorInfo.physicalDevice = physicalDeviceSelectorResult.value().physical_device;
    allocatorInfo.device = device.device;
    allocatorInfo.instance = instance.instance;
    allocatorInfo.vulkanApiVersion = VK_API_VERSION_1_3;

    VmaAllocator allocator;
    VK_CHECK(vmaCreateAllocator(&allocatorInfo, &allocator));

    ComputeApp::GetInstance()->Setup(device.device, instance.instance, allocator, window);
    ComputeApp::GetInstance()->Init();

    uint32_t frame = 0;
    uint32_t currentFrame = 0;
    uint32_t imageIndex = 0;
    bool init = true;

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        vkWaitForFences(device, 1, &inFlightFences[currentFrame], VK_TRUE, UINT64_MAX);
        vkResetFences(device, 1, &inFlightFences[currentFrame]);

        vkAcquireNextImageKHR(device, swapchain, UINT64_MAX, imageAvailableSemaphores[currentFrame], VK_NULL_HANDLE,
                              &imageIndex);

        ComputeApp::GetInstance()->Update(frame);

        vkResetCommandBuffer(computeCommandBuffers[currentFrame], 0);

        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

        //
        //
        // Begin recording compute
        vkBeginCommandBuffer(computeCommandBuffers[currentFrame], &beginInfo);
        if (init) {
            ComputeApp::GetInstance()->ComputeQueuInitCommands(computeCommandBuffers[currentFrame]);
        }

        TransitionImage(computeCommandBuffers[currentFrame], images[imageIndex], VK_IMAGE_LAYOUT_UNDEFINED,
                        VK_IMAGE_LAYOUT_GENERAL);
        // RECORD COMPUTE COMMANDS HERE
        ComputeApp::GetInstance()->ComputeQueueCommands(computeCommandBuffers[currentFrame], images[imageIndex],
                                                        imageViews[imageIndex], swapchain.extent);
        // STOP
        vkEndCommandBuffer(computeCommandBuffers[currentFrame]);

        // Render imgui
        ImGui::Render();

        VkRenderingAttachmentInfo colorAttachment{};
        colorAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
        colorAttachment.pNext = nullptr;

        colorAttachment.imageView = imageViews[imageIndex];
        colorAttachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
        colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;

        VkRenderingInfo renderInfo{};
        renderInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
        renderInfo.layerCount = 1;
        renderInfo.colorAttachmentCount = 1;
        renderInfo.pColorAttachments = &colorAttachment;
        renderInfo.renderArea.extent = swapchain.extent;

        vkBeginCommandBuffer(graphicsCommandBuffers[currentFrame], &beginInfo);
        if (init) {
            ComputeApp::GetInstance()->GraphicsQueueInitCommands(graphicsCommandBuffers[currentFrame]);
        }

        ComputeApp::GetInstance()->GraphicsQueueCommands(graphicsCommandBuffers[currentFrame], images[imageIndex],
                                                         imageViews[imageIndex], swapchain.extent);
        TransitionImage(graphicsCommandBuffers[currentFrame], images[imageIndex], VK_IMAGE_LAYOUT_GENERAL,
                        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
        vkCmdBeginRendering(graphicsCommandBuffers[currentFrame], &renderInfo);

        ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), graphicsCommandBuffers[currentFrame]);

        vkCmdEndRendering(graphicsCommandBuffers[currentFrame]);
        TransitionImage(graphicsCommandBuffers[currentFrame], images[imageIndex],
                        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
        vkEndCommandBuffer(graphicsCommandBuffers[currentFrame]);

        VkSemaphore computeWaitSemaphores[] = {imageAvailableSemaphores[currentFrame]};
        VkSemaphore computeSignalSemaphore[] = {computeFinishedSemaphores[currentFrame]};
        VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_ALL_COMMANDS_BIT};

        VkSubmitInfo computeSubmitInfo{};
        computeSubmitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        computeSubmitInfo.commandBufferCount = 1;
        computeSubmitInfo.pCommandBuffers = &computeCommandBuffers[currentFrame];
        computeSubmitInfo.waitSemaphoreCount = 1;
        computeSubmitInfo.pWaitSemaphores = computeWaitSemaphores;
        computeSubmitInfo.pWaitDstStageMask = waitStages;
        computeSubmitInfo.signalSemaphoreCount = 1;
        computeSubmitInfo.pSignalSemaphores = computeSignalSemaphore;

        VK_CHECK(vkQueueSubmit(computeQueue, 1, &computeSubmitInfo, VK_NULL_HANDLE));

        VkSemaphore graphicsWaitSemaphores[] = {computeFinishedSemaphores[currentFrame]};
        VkSemaphore graphicsSignalSemaphores[] = {graphicsFinishedSemaphores[currentFrame]};

        VkSubmitInfo graphicsSubmitInfo{};
        graphicsSubmitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        graphicsSubmitInfo.commandBufferCount = 1;
        graphicsSubmitInfo.pCommandBuffers = &graphicsCommandBuffers[currentFrame];
        graphicsSubmitInfo.waitSemaphoreCount = 1;
        graphicsSubmitInfo.pWaitSemaphores = graphicsWaitSemaphores;
        graphicsSubmitInfo.pWaitDstStageMask = waitStages;
        graphicsSubmitInfo.signalSemaphoreCount = 1;
        graphicsSubmitInfo.pSignalSemaphores = graphicsSignalSemaphores;

        VK_CHECK(vkQueueSubmit(graphicsQueue, 1, &graphicsSubmitInfo, inFlightFences[currentFrame]));

        VkPresentInfoKHR presentInfo{};
        presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

        presentInfo.waitSemaphoreCount = 1;
        presentInfo.pWaitSemaphores = graphicsSignalSemaphores;

        VkSwapchainKHR swapChains[] = {swapchain};
        presentInfo.swapchainCount = 1;
        presentInfo.pSwapchains = swapChains;
        presentInfo.pImageIndices = &imageIndex;

        vkQueuePresentKHR(graphicsQueue, &presentInfo);

        currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
        frame++;
        if (init) {
            init = false;
        }
    }

    vkDeviceWaitIdle(device);

    ComputeApp::GetInstance()->Cleanup();
    ComputeApp::DestroyInstance();

    ImGui_ImplVulkan_Shutdown();

    vkDestroyDescriptorPool(device, imguiPool, nullptr);

    vkDestroyCommandPool(device, computeCommandPool, nullptr);
    vkDestroyCommandPool(device, graphicsCommandPool, nullptr);

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        vkDestroySemaphore(device, computeFinishedSemaphores[i], nullptr);
        vkDestroySemaphore(device, imageAvailableSemaphores[i], nullptr);
        vkDestroySemaphore(device, graphicsFinishedSemaphores[i], nullptr);
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
