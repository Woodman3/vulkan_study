#include <iostream>
#include "GlfwGeneral.hpp"

using namespace vulkan;


int main() {
    if (!InitializeWindow({1280,720}))
        return -1;//来个你讨厌的返回值
    std::cout << std::format("[ InitializeWindow ]\nWindow created successfully!\n");

    fence fence(VK_FENCE_CREATE_SIGNALED_BIT); //以置位状态创建栅栏
    semaphore semaphore_image_is_available;
    semaphore semaphore_rendering_is_over;
    
    while (!glfwWindowShouldClose(pWindow)) {
        /*渲染过程，待填充*/

        glfwPollEvents();
        TitleFps();
    }
    TerminateWindow();
    return 0;
}