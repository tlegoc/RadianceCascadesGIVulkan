//
// Created by theo on 02/10/2024.
//

#pragma once

#include <Common.h>
#include <imgui_impl_glfw.h>

#define REGISTER_COMPUTE_APP(ComputeAppImpl) \
    ComputeApp *ComputeApp::s_instance = new ComputeAppImpl();

class ComputeApp {
public:
    virtual ~ComputeApp() = default;

    void Setup(VkDevice dev, VkInstance inst, VmaAllocator all, GLFWwindow *win);

    virtual void Init() = 0;

    virtual void Update(uint32_t frame) = 0;

    virtual void ComputeQueueCommands(VkCommandBuffer cmd, VkImage swapchainImage, VkImageView swapchainImageView,
                                VkExtent2D swapchainExtent) {}

    virtual void GraphicsQueueCommands(VkCommandBuffer cmd, VkImage swapchainImage, VkImageView swapchainImageView,
                                 VkExtent2D swapchainExtent) {}

    // Todo : do commands inside render pass

    virtual void Cleanup() = 0;

    static ComputeApp *GetInstance();

    static void DestroyInstance();

protected:
    VkDevice device{};
    VkInstance instance{};
    VmaAllocator allocator{};
    GLFWwindow *window{};

private:
    static ComputeApp *s_instance;
};