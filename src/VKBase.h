#pragma once
#include "EasyVKStart.h"
#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <sys/types.h>
#include <vector>
#include <vulkan/vulkan_core.h>
namespace vulkan {
#define DestroyHandleBy(Func) if (handle) { Func(graphicsBase::Base().Device(), handle, nullptr); handle = VK_NULL_HANDLE; }
#define MoveHandle handle = other.handle; other.handle = VK_NULL_HANDLE;
#define DefineHandleTypeOperator operator auto() const { return handle; }
#define DefineAddressFunction const auto Address() const { return &handle; }

//情况1：根据函数返回值确定是否抛异常
#ifdef VK_RESULT_THROW
class result_t{
    public:
    VkResult result;
    static void(*callback_throw)(VkResult);
    result_t(VkResult result) :result(result) {}
    result_t(result_t&& other) noexcept :result(other.result) { other.result = VK_SUCCESS; }
    ~result_t() noexcept(false) {
        if (uint32_t(result) < VK_RESULT_MAX_ENUM)
            return;
        if (callback_throw)
            callback_throw(result);
        throw result;
    }
    operator VkResult() {
        VkResult result = this->result;
        this->result = VK_SUCCESS;
        return result;
    }
}
inline void(*result_t::callback_throw)(VkResult);
//情况2：若抛弃函数返回值，让编译器发出警告
#elif defined VK_RESULT_NODISCARD
struct [[nodiscard]] result_t {
    VkResult result;
    result_t(VkResult result) :result(result) {}
    operator VkResult() const { return result; }
};
//在本文件中关闭弃值提醒（因为我懒得做处理）
#pragma warning(disable:4834)
#pragma warning(disable:6031)

//情况3：啥都不干
#else
using result_t = VkResult;
#endif

inline auto& outStream = std::cout;//不是constexpr，因为std::cout具有外部链接

constexpr VkExtent2D default_window_size = {
    .width = 1280,
    .height = 720 
};


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

    std::vector<void(*)()> callback_create_swapchain;
    std::vector<void(*)()> callback_destroy_swapchain;
    std::vector<void(*)()> callback_create_device;
    std::vector<void(*)()> callback_destroy_device;

    graphicsBase() = default;
    graphicsBase(graphicsBase&&)  = delete;
    
    public:

    ~graphicsBase() {
        if(!instance)
            return;
        if(device){
            wait_idle();
            if(swapchain) {
                for(auto& i:callback_destroy_swapchain) {
                    i();
                }
                for(auto& i:swapchain_image_views) {
                    if(i)
                        vkDestroyImageView(device, i, nullptr);
                }
                swapchain_image_views.resize(0);
                vkDestroySwapchainKHR(device, swapchain, nullptr);
            }
            for(auto& i:callback_destroy_device) {
                i();
            }
            vkDestroyDevice(device, nullptr);
        }
        if(surface)
            vkDestroySurfaceKHR(instance, surface, nullptr);
        if(debug_messenger) {
            PFN_vkDestroyDebugUtilsMessengerEXT vkDestroyDebugUtilsMessenger =
                reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT"));
            if(vkDestroyDebugUtilsMessenger)
                vkDestroyDebugUtilsMessenger(instance, debug_messenger, nullptr);
        }
        vkDestroyInstance(instance, nullptr);
    }

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
            outStream << std::format("[ graphicsBase ] ERROR\nFailed to create a vulkan instance!\nError code: {}\n", int32_t(result));
            return result;
        }
        //成功创建Vulkan实例后，输出Vulkan版本
        outStream << std::format(
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
                outStream << std::format("{}\n\n", pCallbackData->pMessage);
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
                outStream << std::format("[ graphicsBase ] ERROR\nFailed to create a debug messenger!\nError code: {}\n", int32_t(result));
            return result;
        }
        outStream << std::format("[ graphicsBase ] ERROR\nFailed to get the function pointer of vkCreateDebugUtilsMessengerEXT!\n");
        return VK_RESULT_MAX_ENUM;
    }

    VkResult check_instance_layers(std::span<const char*> layer_to_check) {
        std::vector<VkLayerProperties> available_layers;
        uint32_t layer_count;
        if(VkResult result = vkEnumerateInstanceLayerProperties(&layer_count, nullptr)) {
            outStream << std::format("[ graphicsBase ] ERROR\nFailed to enumerate instance layers!\nError code: {}\n", int32_t(result));
            return result;
        }
        if(layer_count){
            available_layers.resize(layer_count);
            if(VkResult result = vkEnumerateInstanceLayerProperties(&layer_count, available_layers.data())) {
                outStream << std::format("[ graphicsBase ] ERROR\nFailed to enumerate instance layers!\nError code: {}\n", int32_t(result));
                return result;
            }
            for(auto& i:layer_to_check ){
                if(std::find_if(available_layers.begin(), available_layers.end(), [i](const VkLayerProperties& layer) {
                    return std::strcmp(layer.layerName, i) == 0;}) == available_layers.end()){
                    outStream << std::format("[ graphicsBase ] ERROR\nLayer {} not found!\n", i);
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
            outStream<< std::format("[ graphicsBase ] ERROR\nFailed to enumerate instance extensions for layer {}!\nError code: {}\n", layer_name, int32_t(result)) :
            outStream<< std::format("[ graphicsBase ] ERROR\nFailed to enumerate instance extensions!\nError code: {}\n", int32_t(result));
            return result;
        }
        if(extension_count){
            available_extensions.resize(extension_count);
            if(VkResult result = vkEnumerateInstanceExtensionProperties(nullptr, &extension_count, available_extensions.data())) {
                outStream << std::format("[ graphicsBase ] ERROR\nFailed to enumerate instance extensions!\nError code: {}\n", int32_t(result));
                return result;
            }
            for(auto& i:extension_to_check ){
                if(std::find_if(available_extensions.begin(), available_extensions.end(), [i](const VkExtensionProperties& extension) {
                    return std::strcmp(extension.extensionName, i) == 0;}) == available_extensions.end()){
                    outStream << std::format("[ graphicsBase ] ERROR\nExtension {} not found!\n", i);
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
            outStream << std::format("[ graphicsBase ] ERROR\nFailed to enumerate physical devices!\nError code: {}\n", int32_t(result));
            return result;
        }
        if(!device_count){
            outStream << std::format("[ graphicsBase ] ERROR\nNo physical devices found!\n");
            abort();
        }
        available_physical_devices.resize(device_count);
        VkResult result = vkEnumeratePhysicalDevices(instance, &device_count, available_physical_devices.data());
        if(result) {
            outStream << std::format("[ graphicsBase ] ERROR\nFailed to enumerate physical devices!\nError code: {}\n", int32_t(result));
        }
        return result;
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
                return result;
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
        VkPhysicalDeviceFeatures device_features;
        vkGetPhysicalDeviceFeatures(physical_device, &device_features);
        VkDeviceCreateInfo deviceCreateInfo = {
            .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
            .flags = flags,
            .queueCreateInfoCount = queue_create_info_count,
            .pQueueCreateInfos = queue_create_infos,
            .enabledExtensionCount = uint32_t(device_extensions.size()),
            .ppEnabledExtensionNames = device_extensions.data(),
            .pEnabledFeatures = &device_features
        };
        if(VkResult result = vkCreateDevice(physical_device, &deviceCreateInfo, nullptr, &device)) {
            outStream << std::format("[ graphicsBase ] ERROR\nFailed to create a device!\nError code: {}\n", int32_t(result));
            return result;
        }
        if(queue_family_index_graphics != VK_QUEUE_FAMILY_IGNORED) 
            vkGetDeviceQueue(device, queue_family_index_graphics, 0, &queue_graphics);
        if(queue_family_index_compute != VK_QUEUE_FAMILY_IGNORED) 
            vkGetDeviceQueue(device, queue_family_index_compute, 0, &queue_compute);
        if(queue_family_index_presentation != VK_QUEUE_FAMILY_IGNORED)
            vkGetDeviceQueue(device, queue_family_index_presentation, 0, &queue_presentation);
        vkGetPhysicalDeviceProperties(physical_device, &physical_device_properties);
        vkGetPhysicalDeviceMemoryProperties(physical_device, &physical_device_memory_properties);
        outStream << std::format(
            "Physical Device: {}\n",
            physical_device_properties.deviceName);
        for(auto& i:callback_create_device) {
            i();
        }
        return VK_SUCCESS;
    }

    VkResult create_swapchain(bool limit_frame_rate = true,VkSwapchainCreateFlagsKHR flags = 0){
        VkSurfaceCapabilitiesKHR surface_capabilities;
        if(VkResult result = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physical_device, surface, &surface_capabilities)) {
            outStream << std::format("[ graphicsBase ] ERROR\nFailed to get physical device surface capabilities!\nError code: {}\n", int32_t(result));
            return result;
        }
        swapchain_create_info.minImageCount = surface_capabilities.minImageCount + (surface_capabilities.maxImageCount > 0 ? 1 : 0);
        swapchain_create_info.imageExtent = surface_capabilities.currentExtent.width == -1 ?
            VkExtent2D{std::clamp(default_window_size.width, surface_capabilities.minImageExtent.width,surface_capabilities.maxImageExtent.width),
                        std::clamp(default_window_size.height, surface_capabilities.minImageExtent.height,surface_capabilities.maxImageExtent.height)} :
            surface_capabilities.currentExtent;
        swapchain_create_info.imageArrayLayers = 1;
        swapchain_create_info.preTransform = surface_capabilities.currentTransform;
        if(surface_capabilities.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR)
            swapchain_create_info.compositeAlpha = VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR;
        else{
            for(uint32_t i = 1; i < 4; i <<= 1) {
                if(surface_capabilities.supportedCompositeAlpha & i) {
                    swapchain_create_info.compositeAlpha = VkCompositeAlphaFlagBitsKHR(i);
                    break;
                }
            }
        } 
        swapchain_create_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        if(surface_capabilities.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_SRC_BIT)
            swapchain_create_info.imageUsage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
        if(surface_capabilities.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_DST_BIT)
            swapchain_create_info.imageUsage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        else
            outStream<< std::format("[ graphicsBase ] WARNING\nSwapchain image usage flags are not supported!\n");
        if(available_surface_formats.empty()){
            if(VkResult result = get_surface_formats()) {
                outStream << std::format("[ graphicsBase ] ERROR\nFailed to get physical device surface formats!\nError code: {}\n", int32_t(result));
                return result;
            }
        }
        if(!swapchain_create_info.imageFormat){
            if(set_surface_formats({VK_FORMAT_R8G8B8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR}) &&
                set_surface_formats({VK_FORMAT_B8G8R8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR})){
                    outStream << std::format("[ graphicsBase ] WARNING\nSwapchain image format not supported!\n");
                    swapchain_create_info.imageFormat = available_surface_formats[0].format;
                    swapchain_create_info.imageColorSpace = available_surface_formats[0].colorSpace;
            }
        }
        uint32_t surface_present_mode_count;
        if(VkResult result = vkGetPhysicalDeviceSurfacePresentModesKHR(physical_device, surface, &surface_present_mode_count, nullptr)) {
            outStream << std::format("[ graphicsBase ] ERROR\nFailed to get physical device surface present modes!\nError code: {}\n", int32_t(result));
            return result;
        }
        if(!surface_present_mode_count){
            outStream << std::format("[ graphicsBase ] ERROR\nNo surface present modes found!\n");
            abort();
        }
        std::vector<VkPresentModeKHR> surface_present_modes(surface_present_mode_count);
        if(VkResult result = vkGetPhysicalDeviceSurfacePresentModesKHR(physical_device, surface, &surface_present_mode_count, surface_present_modes.data())) {
            outStream << std::format("[ graphicsBase ] ERROR\nFailed to get physical device surface present modes!\nError code: {}\n", int32_t(result));
            return result;
        }
        swapchain_create_info.presentMode = VK_PRESENT_MODE_FIFO_KHR;
        if(!limit_frame_rate) {
            for(auto& i:surface_present_modes) {
                if(i == VK_PRESENT_MODE_MAILBOX_KHR) {
                    swapchain_create_info.presentMode = i;
                    break;
                }
            }
        }
        swapchain_create_info.clipped = VK_TRUE;
        swapchain_create_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        swapchain_create_info.surface = surface;
        swapchain_create_info.flags = flags;
        swapchain_create_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        if(VkResult result = create_swapchain_internal()) {
            outStream << std::format("[ graphicsBase ] ERROR\nFailed to create swapchain!\nError code: {}\n", int32_t(result));
            return result;
        }
        for(auto& i:callback_create_swapchain) {
            i();
        }

        return VK_SUCCESS;
    }

    VkResult recreate_swapchain() {
        VkSurfaceCapabilitiesKHR surface_capabilities;
        if(VkResult result = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physical_device, surface, &surface_capabilities)) {
            outStream << std::format("[ graphicsBase ] ERROR\nFailed to get physical device surface capabilities!\nError code: {}\n", int32_t(result));
            return result;
        }
        if(surface_capabilities.currentExtent.width == -1 || surface_capabilities.currentExtent.height == -1) 
            return VK_SUBOPTIMAL_KHR;
        swapchain_create_info.imageExtent = surface_capabilities.currentExtent;
        swapchain_create_info.oldSwapchain = swapchain;
        VkResult result = vkQueueWaitIdle(queue_graphics);
        if(!result || queue_graphics != queue_presentation) {
            result = vkQueueWaitIdle(queue_presentation);
        } 
        if(result) {
            outStream << std::format("[ graphicsBase ] ERROR\nFailed to wait for queue idle!\nError code: {}\n", int32_t(result));
            return result;
        }
        for(auto& i:callback_destroy_swapchain) {
            i();
        }
        for(auto& i:swapchain_image_views) {
            if(i)
                vkDestroyImageView(device, i, nullptr);
        }
        swapchain_image_views.resize(0);
        if(result = create_swapchain_internal();result) {
            outStream << std::format("[ graphicsBase ] ERROR\nFailed to recreate swapchain!\nError code: {}\n", int32_t(result));
            return result;
        }
        for(auto& i:callback_create_swapchain) {
            i();
        }
        return VK_SUCCESS;
    }

    VkResult recreate_device(VkDeviceCreateFlags flags = 0) {
        if(VkResult result = wait_idle()) {
            outStream << std::format("[ graphicsBase ] ERROR\nFailed to wait for device idle!\nError code: {}\n", int32_t(result));
            return result;
        }
        if(swapchain) {
            for(auto& i:callback_destroy_swapchain) {
                i();
            }
            for(auto& i:swapchain_image_views) {
                if(i)
                    vkDestroyImageView(device, i, nullptr);
            }
            swapchain_image_views.resize(0);
            vkDestroySwapchainKHR(device, swapchain, nullptr);
            swapchain = VK_NULL_HANDLE;
            swapchain_create_info = {};
        }
        for(auto& i:callback_destroy_device) {
            i();
        }
        vkDestroyDevice(device, nullptr);
        device = VK_NULL_HANDLE;
        return create_device(flags);
    }

    private:

    VkResult set_surface_formats(const VkSurfaceFormatKHR& surface_format) {
        bool format_available = false;
        if(!surface_format.format){
            for(auto& i:available_surface_formats) {
                if(i.colorSpace == surface_format.colorSpace) {
                    format_available = true;
                    swapchain_create_info.imageFormat = i.format;
                    swapchain_create_info.imageColorSpace = i.colorSpace; 
                    break;
                }
            }
        }else{
            for(auto& i:available_surface_formats) {
                if(i.format == surface_format.format && i.colorSpace == surface_format.colorSpace) {
                    format_available = true;
                    swapchain_create_info.imageFormat = i.format;
                    swapchain_create_info.imageColorSpace = i.colorSpace; 
                    break;
                }
            }
        }
        if(!format_available) {
            outStream << std::format("[ graphicsBase ] ERROR\nSurface format not supported!\n");
            return VK_ERROR_FORMAT_NOT_SUPPORTED;
        }
        if(swapchain){
            return recreate_swapchain();
        }
        return VK_SUCCESS;
    }

    VkResult wait_idle() const {
        VkResult result = vkDeviceWaitIdle(device);
        if(result) {
            outStream << std::format("[ graphicsBase ] ERROR\nFailed to wait for device idle!\nError code: {}\n", int32_t(result));
        }
        return result;
    }

    VkResult create_swapchain_internal() {
        if(VkResult result = vkCreateSwapchainKHR(device, &swapchain_create_info, nullptr, &swapchain)) {
            outStream << std::format("[ graphicsBase ] ERROR\nFailed to create swapchain!\nError code: {}\n", int32_t(result));
            return result;
        }
        uint32_t swapchain_image_count;
        if(VkResult result = vkGetSwapchainImagesKHR(device, swapchain, &swapchain_image_count, nullptr)) {
            outStream << std::format("[ graphicsBase ] ERROR\nFailed to get swapchain images!\nError code: {}\n", int32_t(result));
            return result;
        }
        if(!swapchain_image_count){
            outStream << std::format("[ graphicsBase ] ERROR\nNo swapchain images found!\n");
            abort();
        }
        swapchain_images.resize(swapchain_image_count);
        if(VkResult result = vkGetSwapchainImagesKHR(device, swapchain, &swapchain_image_count, swapchain_images.data())) {
            outStream << std::format("[ graphicsBase ] ERROR\nFailed to get swapchain images!\nError code: {}\n", int32_t(result));
            return result;
        }
        swapchain_image_views.resize(swapchain_image_count);
        VkImageViewCreateInfo swapchain_image_view_create_info = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .viewType = VK_IMAGE_VIEW_TYPE_2D,
            .format = swapchain_create_info.imageFormat,
            .components = {
                .r = VK_COMPONENT_SWIZZLE_IDENTITY,
                .g = VK_COMPONENT_SWIZZLE_IDENTITY,
                .b = VK_COMPONENT_SWIZZLE_IDENTITY,
                .a = VK_COMPONENT_SWIZZLE_IDENTITY
            },
            .subresourceRange = {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .baseMipLevel = 0,
                .levelCount = 1,
                .baseArrayLayer = 0,
                .layerCount = 1
            }
        };
        for(uint32_t i = 0; i < swapchain_image_count; i++) {
            swapchain_image_view_create_info.image = swapchain_images[i];
            if(VkResult result = vkCreateImageView(device, &swapchain_image_view_create_info, nullptr, &swapchain_image_views[i])) {
                outStream << std::format("[ graphicsBase ] ERROR\nFailed to create swapchain image view!\nError code: {}\n", int32_t(result));
                return result;
            }
        }
        return VK_SUCCESS;
    }

    static void add_layer_or_extension(std::vector<const char*>& vec, const char* name) {
        if(std::find_if(vec.begin(), vec.end(), [name](const char* str) {
            return std::strcmp(str, name) == 0;}) == vec.end()){
            vec.push_back(name);
        }
    }

    VkResult get_surface_formats() {
        uint32_t format_count;
        if(VkResult result = vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device, surface, &format_count, nullptr)) {
            outStream << std::format("[ graphicsBase ] ERROR\nFailed to get physical device surface formats!\nError code: {}\n", int32_t(result));
            return result;
        }
        if(!format_count){
            outStream << std::format("[ graphicsBase ] ERROR\nNo surface formats found!\n");
            abort();
        }
        available_surface_formats.resize(format_count);
        VkResult result = vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device, surface, &format_count, available_surface_formats.data());
        if(result) {
            outStream << std::format("[ graphicsBase ] ERROR\nFailed to get physical device surface formats!\nError code: {}\n", int32_t(result));
        }
        return result;
    }
    VkResult get_queue_family_indices(VkPhysicalDevice physical_device,bool enable_graphics_queue,bool enable_compute_queue,uint32_t (&queue_family_indices)[3]) {
        uint32_t queue_family_count=0;
        vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &queue_family_count, nullptr);
        if(!queue_family_count){
            outStream << std::format("[ graphicsBase ] ERROR\nNo queue family found!\n");
            return VK_RESULT_MAX_ENUM;
        }
        std::vector<VkQueueFamilyProperties> queue_family_properties(queue_family_count);
        vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &queue_family_count, queue_family_properties.data());
        auto& [ig,ip,ic] = queue_family_indices;
        ig = ip = ic = VK_QUEUE_FAMILY_IGNORED;
        for (uint32_t i = 0; i < queue_family_count; i++) {
            VkBool32 support_graphics = enable_graphics_queue && queue_family_properties[i].queueFlags & VK_QUEUE_GRAPHICS_BIT,
                support_compute = enable_compute_queue && queue_family_properties[i].queueFlags & VK_QUEUE_COMPUTE_BIT,
                support_presentation = false;

            if(surface) {
                if(VkResult result = vkGetPhysicalDeviceSurfaceSupportKHR(physical_device, i, surface, &support_presentation)) {
                    outStream << std::format("[ graphicsBase ] ERROR\nFailed to get physical device surface support!\nError code: {}\n", int32_t(result));
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

};

inline graphicsBase graphics_base;

class fence {
    VkFence handle = VK_NULL_HANDLE;
public:
    //fence() = default;
    fence(VkFenceCreateInfo& createInfo) {
        create(createInfo);
    }
    //默认构造器创建未置位的栅栏
    fence(VkFenceCreateFlags flags = 0) {
        create(flags);
    }
    fence(fence&& other) noexcept { MoveHandle; }
    ~fence() { DestroyHandleBy(vkDestroyFence); }
    //Getter
    DefineHandleTypeOperator;
    DefineAddressFunction;
    //Const Function
    result_t wait() const {
        VkResult result = vkWaitForFences(graphics_base.device, 1, &handle, false, UINT64_MAX);
        if (result)
            outStream << std::format("[ fence ] ERROR\nFailed to wait for the fence!\nError code: {}\n", int32_t(result));
        return result;
    }
    result_t reset() const {
        VkResult result = vkResetFences(graphics_base.device, 1, &handle);
        if (result)
            outStream << std::format("[ fence ] ERROR\nFailed to reset the fence!\nError code: {}\n", int32_t(result));
        return result;
    }
    //因为“等待后立刻重置”的情形经常出现，定义此函数
    result_t wait_and_reset() const {
        VkResult result = wait();
        result || (result = reset());
        return result;
    }
    result_t status() const {
        VkResult result = vkGetFenceStatus(graphics_base.device, handle);
        if (result < 0) //vkGetFenceStatus(...)成功时有两种结果，所以不能仅仅判断result是否非0
            outStream << std::format("[ fence ] ERROR\nFailed to get the status of the fence!\nError code: {}\n", int32_t(result));
        return result;
    }
    //Non-const Function
    result_t create(VkFenceCreateInfo& createInfo) {
        createInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        VkResult result = vkCreateFence(graphics_base.device, &createInfo, nullptr, &handle);
        if (result)
            outStream << std::format("[ fence ] ERROR\nFailed to create a fence!\nError code: {}\n", int32_t(result));
        return result;
    }
    result_t create(VkFenceCreateFlags flags = 0) {
        VkFenceCreateInfo createInfo = {
            .flags = flags
        };
        return create(createInfo);
    }
};

class semaphore {
    VkSemaphore handle = VK_NULL_HANDLE;
public:
    //semaphore() = default;
    semaphore(VkSemaphoreCreateInfo& createInfo) {
        create(createInfo);
    }
    //默认构造器创建未置位的信号量
    semaphore(/*VkSemaphoreCreateFlags flags*/) {
        create();
    }
    semaphore(semaphore&& other) noexcept { MoveHandle; }
    ~semaphore() { DestroyHandleBy(vkDestroySemaphore); }
    //Getter
    DefineHandleTypeOperator;
    DefineAddressFunction;
    //Non-const Function
    result_t create(VkSemaphoreCreateInfo& createInfo) {
        createInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
        VkResult result = vkCreateSemaphore(graphics_base.device, &createInfo, nullptr, &handle);
        if (result)
            outStream << std::format("[ semaphore ] ERROR\nFailed to create a semaphore!\nError code: {}\n", int32_t(result));
        return result;
    }
    result_t create(/*VkSemaphoreCreateFlags flags*/) {
        VkSemaphoreCreateInfo createInfo = {};
        return create(createInfo);
    }
};


}