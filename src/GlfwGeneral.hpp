#include "VKBase.h"
#include <vulkan/vulkan_core.h>
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
// #pragma comment(lib, "glfw3.lib") //链接编译所需的静态库

//窗口的指针，全局变量自动初始化为NULL
GLFWwindow* pWindow;
//显示器信息的指针
GLFWmonitor* pMonitor;
//窗口标题
const char* windowTitle = "EasyVK";

bool InitializeWindow(VkExtent2D size, bool fullScreen = false, bool isResizable = true, bool limitFrameRate = true) {
    using vulkan::graphics_base;

    if (!glfwInit()) {
        std::cout << std::format("[ InitializeWindow ] ERROR\nFailed to initialize GLFW!\n");
        return false;
    }
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, isResizable);
    pMonitor = glfwGetPrimaryMonitor();
    const GLFWvidmode* pMode = glfwGetVideoMode(pMonitor);
    pWindow = fullScreen ?
        glfwCreateWindow(pMode->width, pMode->height, windowTitle, pMonitor, nullptr) :
        glfwCreateWindow(size.width, size.height, windowTitle, nullptr, nullptr);
    if (!pWindow) {
        std::cout << std::format("[ InitializeWindow ]\nFailed to create a glfw window!\n");
        glfwTerminate();
        return false;
    }
    
    uint32_t extensionCount = 0;
    const char** extensionNames;
    extensionNames = glfwGetRequiredInstanceExtensions(&extensionCount);
    if (!extensionNames) {
        std::cout << std::format("[ InitializeWindow ]\nVulkan is not available on this machine!\n");
        glfwTerminate();
        return false;
    }
    for (size_t i = 0; i < extensionCount; i++){
        std::cout << std::format("[ InitializeWindow ]\nExtension {}: {}\n", i, extensionNames[i]);
        graphics_base.add_instance_extension(extensionNames[i]);
    }
    graphics_base.add_device_extension(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
    if(graphics_base.create_instance()) {
        std::cout << std::format("[ InitializeWindow ] ERROR\nFailed to create a Vulkan instance!\n");
        return false;
    }
    VkSurfaceKHR surface = VK_NULL_HANDLE;
    if(VkResult result = glfwCreateWindowSurface(graphics_base.instance, pWindow, nullptr, &surface)) {
        std::cout << std::format("[ InitializeWindow ] ERROR\nFailed to create a window surface!\nError code: {}\n", int32_t(result));
        glfwDestroyWindow(pWindow);
        glfwTerminate();
        return false;
    }
    graphics_base.surface = surface;
    if(graphics_base.get_physical_devices() ||
        graphics_base.determine_physical_device(0,true,false) ||
        graphics_base.create_device()) {

        std::cout << std::format("[ InitializeWindow ] ERROR\nFailed to create a Vulkan device!\n");
        return false;
    }

    /*待Ch1-3和Ch1-4填充*/
    return true;
}
void TerminateWindow() {
    /*待Ch1-4填充*/
    glfwTerminate();
}
void TitleFps() {
    static double time0 = glfwGetTime();
    static double time1;
    static double dt;
    static int dframe = -1;
    static std::stringstream info;
    time1 = glfwGetTime();
    dframe++;
    if ((dt = time1 - time0) >= 1) {
        info.precision(1);
        info << windowTitle << "    " << std::fixed << dframe / dt << " FPS";
        glfwSetWindowTitle(pWindow, info.str().c_str());
        info.str("");//别忘了在设置完窗口标题后清空所用的stringstream
        time0 = time1;
        dframe = 0;
    }
}
