#pragma once
#include "EasyVKStart.h"
#include <vulkan/vulkan_core.h>
//定义vulkan命名空间，之后会把Vulkan中一些基本对象的封装写在其中
namespace vulkan {
    class graphicsBase {
        public:
            uint32_t api_version = VK_API_VERSION_1_0;

            //单例类对象是静态的，未设定初始值亦无构造函数的成员会被零初始化
            VkInstance instance;
            std::vector<const char*> instance_layers;
            std::vector<const char*> instance_extensions;

            VkSurfaceKHR surface;
            VkDebugUtilsMessengerEXT debug_messenger;
            VkPhysicalDevice physical_device;
            VkPhysicalDeviceProperties physical_device_properties;
            VkPhysicalDeviceMemoryProperties physical_device_memory_properties;
            std::vector<VkPhysicalDevice> available_physical_devices;

            VkDevice device;
            // 有效的索引从0开始，因此使用特殊值VK_QUEUE_FAMILY_IGNORED（为UINT32_MAX）为队列族索引的默认值
            uint32_t queue_family_index_graphics = VK_QUEUE_FAMILY_IGNORED;
            uint32_t queue_family_index_presentation = VK_QUEUE_FAMILY_IGNORED;
            uint32_t queue_family_index_compute = VK_QUEUE_FAMILY_IGNORED;
            VkQueue queue_graphics;
            VkQueue queue_presentation;
            VkQueue queue_compute;

            std::vector<const char*> device_extensions;

            std::vector<VkSurfaceFormatKHR> available_surface_formats;

            VkSwapchainKHR swapchain;
            std::vector<VkImage> swapchain_images;
            std::vector<VkImageView> swapchain_image_views;
            // 保存交换链的创建信息以便重建交换链
            VkSwapchainCreateInfoKHR swapchain_create_info = {};
            graphicsBase() = default;
            graphicsBase(graphicsBase&&)  = delete;
        
        public:
            void add_instance_layer(const char* layer) {
                add_layer_or_extension(instance_layers, layer);
            }
            void add_instance_extension(const char* extension) {
                add_layer_or_extension(instance_extensions, extension);
            }
            void add_device_extension(const char* extension) {
                add_layer_or_extension(device_extensions, extension);
            }

            VkResult use_latest_version() {
                if (vkGetInstanceProcAddr(VK_NULL_HANDLE, "vkEnumerateInstanceVersion"))
                    return vkEnumerateInstanceVersion(&api_version);
                return VK_SUCCESS;
            }


            VkResult create_instance(VkInstanceCreateFlags flags = 0){
            #ifndef NDEBUG
                add_instance_layer("VK_LAYER_KHRONOS_validation");
                add_instance_extension(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
            #endif
            VkApplicationInfo applicatianInfo = {
                    .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
                    .apiVersion = api_version
                };
            VkInstanceCreateInfo instanceCreateInfo = {
                .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
                .flags = flags,
                .pApplicationInfo = &applicatianInfo,
                .enabledLayerCount = uint32_t(instance_layers.size()),
                .ppEnabledLayerNames = instance_layers.data(),
                .enabledExtensionCount = uint32_t(instance_extensions.size()),
                .ppEnabledExtensionNames = instance_extensions.data()
            };
            if (VkResult result = vkCreateInstance(&instanceCreateInfo, nullptr, &instance)) {
                std::cout << std::format("[ graphicsBase ] ERROR\nFailed to create a vulkan instance!\nError code: {}\n", int32_t(result));
                return result;
            }
            //成功创建Vulkan实例后，输出Vulkan版本
            std::cout << std::format(
                "Vulkan API Version: {}.{}.{}\n",
                VK_VERSION_MAJOR(api_version),
                VK_VERSION_MINOR(api_version),
                VK_VERSION_PATCH(api_version));
            #ifndef NDEBUG
                //创建完Vulkan实例后紧接着创建debug messenger
                create_debug_messenger();
            #endif
                return VK_SUCCESS;
            }

            VkResult create_debug_messenger(){
                static PFN_vkDebugUtilsMessengerCallbackEXT DebugUtilsMessengerCallback = [](
                    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
                    VkDebugUtilsMessageTypeFlagsEXT messageTypes,
                    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
                    void* pUserData)->VkBool32 {
                        std::cout << std::format("{}\n\n", pCallbackData->pMessage);
                        // 文档说必须返回VK_FALSE,VK_TRUE留作其他用处
                        return VK_FALSE;
                };
                VkDebugUtilsMessengerCreateInfoEXT debugUtilsMessengerCreateInfo = {
                    .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
                    .messageSeverity =
                        VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                        VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
                    .messageType =
                        VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                        VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                        VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
                    .pfnUserCallback = DebugUtilsMessengerCallback
                };
                PFN_vkCreateDebugUtilsMessengerEXT vkCreateDebugUtilsMessenger =
                    reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT"));
                if (vkCreateDebugUtilsMessenger) {
                    VkResult result = vkCreateDebugUtilsMessenger(instance, &debugUtilsMessengerCreateInfo, nullptr, &debug_messenger);
                    if (result)
                        std::cout << std::format("[ graphicsBase ] ERROR\nFailed to create a debug messenger!\nError code: {}\n", int32_t(result));
                    return result;
                }
                std::cout << std::format("[ graphicsBase ] ERROR\nFailed to get the function pointer of vkCreateDebugUtilsMessengerEXT!\n");
                return VK_RESULT_MAX_ENUM;
            }

            VkResult check_instance_layers(std::span<const char*> layer_to_check) {
                std::vector<VkLayerProperties> available_layers;
                uint32_t layer_count;
                if(VkResult result = vkEnumerateInstanceLayerProperties(&layer_count, nullptr)) {
                    std::cout << std::format("[ graphicsBase ] ERROR\nFailed to enumerate instance layers!\nError code: {}\n", int32_t(result));
                    return result;
                }
                if(layer_count){
                    available_layers.resize(layer_count);
                    if(VkResult result = vkEnumerateInstanceLayerProperties(&layer_count, available_layers.data())) {
                        std::cout << std::format("[ graphicsBase ] ERROR\nFailed to enumerate instance layers!\nError code: {}\n", int32_t(result));
                        return result;
                    }
                    for(auto& i:layer_to_check ){
                        if(std::find_if(available_layers.begin(), available_layers.end(), [i](const VkLayerProperties& layer) {
                            return strcmp(layer.layerName, i) == 0;}) == available_layers.end()){
                            std::cout << std::format("[ graphicsBase ] ERROR\nLayer {} not found!\n", i);
                            i = nullptr;
                        }
                    }
                }else{
                    for(auto& i:layer_to_check ){
                        i = nullptr;
                    }
                }
                return VK_SUCCESS;
            }

            VkResult check_instance_extensions(std::span<const char*> extension_to_check,const char* layer_name) {
                std::vector<VkExtensionProperties> available_extensions;
                uint32_t extension_count;
                if(VkResult result = vkEnumerateInstanceExtensionProperties(layer_name, &extension_count, nullptr)) {
                    layer_name ? 
                    std::cout<< std::format("[ graphicsBase ] ERROR\nFailed to enumerate instance extensions for layer {}!\nError code: {}\n", layer_name, int32_t(result)) :
                    std::cout<< std::format("[ graphicsBase ] ERROR\nFailed to enumerate instance extensions!\nError code: {}\n", int32_t(result));
                    return result;
                }
                if(extension_count){
                    available_extensions.resize(extension_count);
                    if(VkResult result = vkEnumerateInstanceExtensionProperties(nullptr, &extension_count, available_extensions.data())) {
                        std::cout << std::format("[ graphicsBase ] ERROR\nFailed to enumerate instance extensions!\nError code: {}\n", int32_t(result));
                        return result;
                    }
                    for(auto& i:extension_to_check ){
                        if(std::find_if(available_extensions.begin(), available_extensions.end(), [i](const VkExtensionProperties& extension) {
                            return strcmp(extension.extensionName, i) == 0;}) == available_extensions.end()){
                            std::cout << std::format("[ graphicsBase ] ERROR\nExtension {} not found!\n", i);
                            i = nullptr;
                        }
                    }
                }else{
                    for(auto& i:extension_to_check ){
                        i = nullptr;
                    }
                }
                return VK_SUCCESS;
            }

        
        private:
            static void add_layer_or_extension(std::vector<const char*>& vec, const char* name) {
                // if (std::find(vec.begin(), vec.end(), name) == vec.end()) {
                if(std::find_if(vec.begin(), vec.end(), [name](const char* str) {
                    return strcmp(str, name) == 0;} == vec.end()){
                    vec.push_back(name);
                }
            }
    };

    inline graphicsBase graphics_base;
}