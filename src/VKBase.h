#pragma once
#include "EasyVKStart.h"
#include <cstdint>
#include <cstdlib>
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

        VkResult get_physical_devices() {
            uint32_t device_count;
            if(VkResult result = vkEnumeratePhysicalDevices(instance, &device_count, nullptr)) {
                std::cout << std::format("[ graphicsBase ] ERROR\nFailed to enumerate physical devices!\nError code: {}\n", int32_t(result));
                return result;
            }
            if(!device_count){
                std::cout << std::format("[ graphicsBase ] ERROR\nNo physical devices found!\n");
                abort();
            }
            available_physical_devices.resize(device_count);
            VkResult result = vkEnumeratePhysicalDevices(instance, &device_count, available_physical_devices.data());
            if(result) {
                std::cout << std::format("[ graphicsBase ] ERROR\nFailed to enumerate physical devices!\nError code: {}\n", int32_t(result));
            }
            return result;
        }

        VkResult get_queue_family_indices(VkPhysicalDevice physical_device,bool enable_graphics_queue,bool enable_compute_queue,uint32_t (&queue_family_indices)[3]) {
            uint32_t queue_family_count=0;
            vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &queue_family_count, nullptr);
            if(!queue_family_count){
                std::cout << std::format("[ graphicsBase ] ERROR\nNo queue family found!\n");
                return VK_RESULT_MAX_ENUM;
            }
            std::vector<VkQueueFamilyProperties> queue_family_properties(queue_family_count);
            vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &queue_family_count, queue_family_properties.data());
            auto& [ig,ip,ic] = queue_family_indices;
            ig = ip = ic = VK_QUEUE_FAMILY_IGNORED;
            for (uint32_t i = 0; i < queue_family_count; i++) {
                VkBool32 support_graphics = enable_compute_queue && queue_family_properties[i].queueFlags & VK_QUEUE_GRAPHICS_BIT,
                    support_compute = enable_graphics_queue && queue_family_properties[i].queueFlags & VK_QUEUE_COMPUTE_BIT,
                    support_presentation = false;

                if(surface) {
                    if(VkResult result = vkGetPhysicalDeviceSurfaceSupportKHR(physical_device, i, surface, &support_presentation)) {
                        std::cout << std::format("[ graphicsBase ] ERROR\nFailed to get physical device surface support!\nError code: {}\n", int32_t(result));
                        return result;
                    }
                }
                if(support_graphics && support_compute) {
                    if(support_presentation){
                        ig = ip = ic =i;
                        break;
                    }
                    if(ig != ic || 
                    ig == VK_QUEUE_FAMILY_IGNORED) {
                        ig = ic = i;
                    }
                    if(!surface){
                        break;
                    }
                }
                if(support_graphics && ig == VK_QUEUE_FAMILY_IGNORED) {
                    ig = i;
                }
                if(support_compute && ic == VK_QUEUE_FAMILY_IGNORED) {
                    ic = i;
                }
                if(support_presentation && ip == VK_QUEUE_FAMILY_IGNORED) {
                    ip = i;
                }

            }
            if(enable_graphics_queue && ig == VK_QUEUE_FAMILY_IGNORED || 
                enable_compute_queue && ic == VK_QUEUE_FAMILY_IGNORED ||
                surface && ip == VK_QUEUE_FAMILY_IGNORED) {
                return VK_RESULT_MAX_ENUM;
            }
            queue_family_index_graphics = ig;
            queue_family_index_compute = ic;
            queue_family_index_presentation = ip;
            return VK_SUCCESS;
        }

        VkResult determine_physical_device(uint32_t device_index = 0 , bool enable_graphics_queue = true, bool enable_compute_queue = true) {
            static constexpr uint32_t not_found = INT32_MAX; // == VK_QUEUE_FAMILY_IGNORED & INT32_MAX
            //定义队列族索引组合的结构体
            struct queueFamilyIndexCombination {
                uint32_t graphics = VK_QUEUE_FAMILY_IGNORED;
                uint32_t presentation = VK_QUEUE_FAMILY_IGNORED;
                uint32_t compute = VK_QUEUE_FAMILY_IGNORED;
            };
            //queueFamilyIndexCombinations用于为各个物理设备保存一份队列族索引组合
            static std::vector<queueFamilyIndexCombination> queueFamilyIndexCombinations(available_physical_devices.size());
            auto& [ig,ip,ic] = queueFamilyIndexCombinations[device_index];
            if(enable_compute_queue && ig == not_found ||
                surface && ip == not_found ||
                enable_graphics_queue && ic == not_found) {
                    return VK_RESULT_MAX_ENUM;
            }
            if(enable_graphics_queue && ig == VK_QUEUE_FAMILY_IGNORED ||
                enable_compute_queue && ic == VK_QUEUE_FAMILY_IGNORED ||
                surface && ip == VK_QUEUE_FAMILY_IGNORED) {

                uint32_t indices[3];
                VkResult result = get_queue_family_indices(available_physical_devices[device_index], enable_graphics_queue, enable_compute_queue, indices);
                if(result == VK_SUCCESS ||
                    result == VK_RESULT_MAX_ENUM){
                    
                    if(enable_graphics_queue)
                        ig = indices[0] & INT32_MAX;
                    if(enable_compute_queue)
                        ic = indices[1] & INT32_MAX;
                    if(surface)
                        ip = indices[2] & INT32_MAX;
                }
                if(result)
                    return result:
            }else{
                queue_family_index_graphics = enable_compute_queue ? ig : VK_QUEUE_FAMILY_IGNORED;
                queue_family_index_compute = enable_graphics_queue ? ic : VK_QUEUE_FAMILY_IGNORED;
                queue_family_index_presentation = surface ? ip : VK_QUEUE_FAMILY_IGNORED;
            }
            physical_device = available_physical_devices[device_index];
            return VK_SUCCESS;
        }
        
        VkResult create_device(VkDeviceCreateFlags flags =0){
            float queue_priority = 1.0f;
            VkDeviceQueueCreateInfo queue_create_infos[3] = {
                {
                    .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
                    .queueCount = 1,
                    .pQueuePriorities = &queue_priority
                },
                {
                    .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
                    .queueCount = 1,
                    .pQueuePriorities = &queue_priority
                },
                {
                    .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
                    .queueCount = 1,
                    .pQueuePriorities = &queue_priority
                }
            };
            uint32_t queue_create_info_count = 0;
            if(queue_family_index_graphics != VK_QUEUE_FAMILY_IGNORED) {
                queue_create_infos[queue_create_info_count].queueFamilyIndex = queue_family_index_graphics;
                queue_create_info_count++;
            }
            if(queue_family_index_compute != VK_QUEUE_FAMILY_IGNORED) {
                queue_create_infos[queue_create_info_count].queueFamilyIndex = queue_family_index_compute;
                queue_create_info_count++;
            }
            if(queue_family_index_presentation != VK_QUEUE_FAMILY_IGNORED) {
                queue_create_infos[queue_create_info_count].queueFamilyIndex = queue_family_index_presentation;
                queue_create_info_count++;
            }
            VkPhsicalDeviceFeatures device_features;
            vkGetPhysicalDeviceFeatures(physical_device, &device_features);
            VkDeviceCreateInfo deviceCreateInfo = {
                .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
                .flags = flags,
                .queueCreateInfoCount = queue_create_info_count,
                .pQueueCreateInfos = queue_create_infos,
                .pEnabledFeatures = &device_features,
                .enabledExtensionCount = uint32_t(device_extensions.size()),
                .ppEnabledExtensionNames = device_extensions.data()
            };
            if(VkResult result = vkCreateDevice(physical_device, &deviceCreateInfo, nullptr, &device)) {
                std::cout << std::format("[ graphicsBase ] ERROR\nFailed to create a device!\nError code: {}\n", int32_t(result));
                return result;
            }
            if(queue_family_index_graphics != VK_QUEUE_FAMILY_IGNORED) 
                vkGetDeviceQueue(device, queue_family_index_graphics, 0, &queue_graphics);
            if(queue_family_index_compute != VK_QUEUE_FAMILY_IGNORED) 
                vkGetDeviceQueue(device, queue_family_index_compute, 0, &queue_compute);
            if(queue_family_index_presentation != VK_QUEUE_FAMILY_IGNORED)
                vkGetDeviceQueue(device, queue_family_index_presentation, 0, &queue_presentation);
            vkPhysicalDeviceProperties(physical_device, &physical_device_properties);
            vkGetPhysicalDeviceMemoryProperties(physical_device, &physical_device_memory_properties);
            std::cout << std::format(
                "Physical Device: {}\n",
                physical_device_properties.deviceName);
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