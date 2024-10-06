//
// Created by theo on 03/10/2024.
//

#include <ComputeApp.h>

void ComputeApp::Setup(VkDevice dev, VkInstance inst, VmaAllocator all, GLFWwindow *win) {
    device = dev;
    instance = inst;
    allocator = all;
    window = win;
}

ComputeApp * ComputeApp::GetInstance() {
    return s_instance;
}

void ComputeApp::DestroyInstance() {
    delete s_instance;
}
