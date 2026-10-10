#include "gfx/VulkanContext.h"

#include "gfx/VkCheck.h"

#include <SDL3/SDL_log.h>
#include <SDL3/SDL_vulkan.h>

#include <cstring>
#include <vector>

namespace gfx {

namespace {

constexpr const char* kValidationLayer = "VK_LAYER_KHRONOS_validation";

#ifdef NDEBUG
constexpr bool kWantValidation = false;
#else
constexpr bool kWantValidation = true;
#endif

VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT severity,
                                             VkDebugUtilsMessageTypeFlagsEXT,
                                             const VkDebugUtilsMessengerCallbackDataEXT* data,
                                             void*)
{
    if (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)
        SDL_LogError(SDL_LOG_CATEGORY_GPU, "[vulkan] %s", data->pMessage);
    else
        SDL_LogWarn(SDL_LOG_CATEGORY_GPU, "[vulkan] %s", data->pMessage);
    return VK_FALSE;
}

VkDebugUtilsMessengerCreateInfoEXT debugMessengerInfo()
{
    VkDebugUtilsMessengerCreateInfoEXT info{VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT};
    info.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                           VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    info.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                       VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                       VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    info.pfnUserCallback = debugCallback;
    return info;
}

bool hasLayer(const char* name)
{
    uint32_t count = 0;
    vkEnumerateInstanceLayerProperties(&count, nullptr);
    std::vector<VkLayerProperties> layers(count);
    vkEnumerateInstanceLayerProperties(&count, layers.data());
    for (const auto& layer : layers)
        if (std::strcmp(layer.layerName, name) == 0)
            return true;
    return false;
}

bool hasDeviceExtension(VkPhysicalDevice device, const char* name)
{
    uint32_t count = 0;
    vkEnumerateDeviceExtensionProperties(device, nullptr, &count, nullptr);
    std::vector<VkExtensionProperties> extensions(count);
    vkEnumerateDeviceExtensionProperties(device, nullptr, &count, extensions.data());
    for (const auto& ext : extensions)
        if (std::strcmp(ext.extensionName, name) == 0)
            return true;
    return false;
}

} // namespace

VulkanContext::VulkanContext(SDL_Window* window)
{
    createInstance();
    createDebugMessenger();

    if (!SDL_Vulkan_CreateSurface(window, instance_, nullptr, &surface_))
        throw std::runtime_error(std::string("SDL_Vulkan_CreateSurface: ") + SDL_GetError());

    pickPhysicalDevice();
    createDevice();
    createAllocator();

    VkCommandPoolCreateInfo poolInfo{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
    poolInfo.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
    poolInfo.queueFamilyIndex = queueFamily_;
    VK_CHECK(vkCreateCommandPool(device_, &poolInfo, nullptr, &uploadPool_));
}

VulkanContext::~VulkanContext()
{
    if (uploadPool_)
        vkDestroyCommandPool(device_, uploadPool_, nullptr);
    if (allocator_)
        vmaDestroyAllocator(allocator_);
    if (device_)
        vkDestroyDevice(device_, nullptr);
    destroySurface();
    if (debugMessenger_) {
        auto destroy = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(
            vkGetInstanceProcAddr(instance_, "vkDestroyDebugUtilsMessengerEXT"));
        destroy(instance_, debugMessenger_, nullptr);
    }
    if (instance_)
        vkDestroyInstance(instance_, nullptr);
}

void VulkanContext::destroySurface()
{
    if (surface_)
        SDL_Vulkan_DestroySurface(instance_, surface_, nullptr);
    surface_ = VK_NULL_HANDLE;
}

bool VulkanContext::createSurface(SDL_Window* window)
{
    if (!SDL_Vulkan_CreateSurface(window, instance_, nullptr, &surface_)) {
        SDL_LogWarn(SDL_LOG_CATEGORY_GPU, "SDL_Vulkan_CreateSurface: %s", SDL_GetError());
        surface_ = VK_NULL_HANDLE;
        return false;
    }
    // The queue was picked for the first surface; a new one must work with it too.
    VkBool32 present = VK_FALSE;
    VK_CHECK(vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice_, queueFamily_, surface_, &present));
    if (!present)
        throw std::runtime_error("The GPU can no longer present to the window");
    return true;
}

void VulkanContext::createInstance()
{
    uint32_t sdlExtCount = 0;
    const char* const* sdlExts = SDL_Vulkan_GetInstanceExtensions(&sdlExtCount);
    if (!sdlExts)
        throw std::runtime_error(std::string("SDL_Vulkan_GetInstanceExtensions: ") + SDL_GetError());
    std::vector<const char*> extensions(sdlExts, sdlExts + sdlExtCount);
    std::vector<const char*> layers;

    if (kWantValidation) {
        if (hasLayer(kValidationLayer)) {
            validationEnabled_ = true;
            layers.push_back(kValidationLayer);
            extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
        } else {
            SDL_LogWarn(SDL_LOG_CATEGORY_GPU,
                        "%s not found; running without validation "
                        "(install the vulkan-validation-layers package)",
                        kValidationLayer);
        }
    }

    VkApplicationInfo appInfo{VK_STRUCTURE_TYPE_APPLICATION_INFO};
    appInfo.pApplicationName = "Galaxians";
    appInfo.applicationVersion = VK_MAKE_VERSION(0, 1, 0);
    appInfo.pEngineName = "none";
    appInfo.apiVersion = VK_API_VERSION_1_3;

    VkInstanceCreateInfo info{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
    info.pApplicationInfo = &appInfo;
    info.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
    info.ppEnabledExtensionNames = extensions.data();
    info.enabledLayerCount = static_cast<uint32_t>(layers.size());
    info.ppEnabledLayerNames = layers.data();

    // Chaining the messenger info also reports problems in vkCreateInstance itself.
    VkDebugUtilsMessengerCreateInfoEXT debugInfo = debugMessengerInfo();
    if (validationEnabled_)
        info.pNext = &debugInfo;

    VK_CHECK(vkCreateInstance(&info, nullptr, &instance_));
}

void VulkanContext::createDebugMessenger()
{
    if (!validationEnabled_)
        return;
    auto create = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(
        vkGetInstanceProcAddr(instance_, "vkCreateDebugUtilsMessengerEXT"));
    VkDebugUtilsMessengerCreateInfoEXT info = debugMessengerInfo();
    VK_CHECK(create(instance_, &info, nullptr, &debugMessenger_));
}

void VulkanContext::pickPhysicalDevice()
{
    uint32_t count = 0;
    vkEnumeratePhysicalDevices(instance_, &count, nullptr);
    std::vector<VkPhysicalDevice> devices(count);
    vkEnumeratePhysicalDevices(instance_, &count, devices.data());

    int bestScore = -1;
    for (VkPhysicalDevice device : devices) {
        VkPhysicalDeviceProperties props;
        vkGetPhysicalDeviceProperties(device, &props);
        if (props.apiVersion < VK_API_VERSION_1_3 ||
            !hasDeviceExtension(device, VK_KHR_SWAPCHAIN_EXTENSION_NAME))
            continue;

        VkPhysicalDeviceVulkan13Features features13{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES};
        VkPhysicalDeviceFeatures2 features{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2};
        features.pNext = &features13;
        vkGetPhysicalDeviceFeatures2(device, &features);
        if (!features13.dynamicRendering || !features13.synchronization2 ||
            !features13.shaderDemoteToHelperInvocation)
            continue;

        // Need a single family that can both draw and present to our surface.
        uint32_t familyCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(device, &familyCount, nullptr);
        std::vector<VkQueueFamilyProperties> families(familyCount);
        vkGetPhysicalDeviceQueueFamilyProperties(device, &familyCount, families.data());

        int family = -1;
        for (uint32_t i = 0; i < familyCount; ++i) {
            VkBool32 present = VK_FALSE;
            vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface_, &present);
            if ((families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) && present) {
                family = static_cast<int>(i);
                break;
            }
        }
        if (family < 0)
            continue;

        int score = props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU     ? 2
                    : props.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU ? 1
                                                                                 : 0;
        if (score > bestScore) {
            bestScore = score;
            physicalDevice_ = device;
            queueFamily_ = static_cast<uint32_t>(family);
        }
    }

    if (!physicalDevice_)
        throw std::runtime_error("No GPU with Vulkan 1.3, dynamic rendering, synchronization2, demote-to-helper and presentation support");

    VkPhysicalDeviceProperties props;
    vkGetPhysicalDeviceProperties(physicalDevice_, &props);
    SDL_Log("Using GPU: %s", props.deviceName);
}

void VulkanContext::createDevice()
{
    float priority = 1.0f;
    VkDeviceQueueCreateInfo queueInfo{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
    queueInfo.queueFamilyIndex = queueFamily_;
    queueInfo.queueCount = 1;
    queueInfo.pQueuePriorities = &priority;

    VkPhysicalDeviceVulkan13Features features13{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES};
    features13.dynamicRendering = VK_TRUE;
    features13.synchronization2 = VK_TRUE;
    // glslc compiles `discard` to OpDemoteToHelperInvocation when targeting Vulkan 1.3.
    features13.shaderDemoteToHelperInvocation = VK_TRUE;

    const char* extensions[] = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};

    VkDeviceCreateInfo info{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
    info.pNext = &features13;
    info.queueCreateInfoCount = 1;
    info.pQueueCreateInfos = &queueInfo;
    info.enabledExtensionCount = 1;
    info.ppEnabledExtensionNames = extensions;

    VK_CHECK(vkCreateDevice(physicalDevice_, &info, nullptr, &device_));
    vkGetDeviceQueue(device_, queueFamily_, 0, &queue_);
}

void VulkanContext::createAllocator()
{
    VmaAllocatorCreateInfo info{};
    info.physicalDevice = physicalDevice_;
    info.device = device_;
    info.instance = instance_;
    info.vulkanApiVersion = VK_API_VERSION_1_3;
    VK_CHECK(vmaCreateAllocator(&info, &allocator_));
}

void VulkanContext::immediateSubmit(const std::function<void(VkCommandBuffer)>& record) const
{
    VkCommandBufferAllocateInfo allocInfo{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    allocInfo.commandPool = uploadPool_;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = 1;
    VkCommandBuffer cmd = VK_NULL_HANDLE;
    VK_CHECK(vkAllocateCommandBuffers(device_, &allocInfo, &cmd));

    VkCommandBufferBeginInfo begin{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    VK_CHECK(vkBeginCommandBuffer(cmd, &begin));
    record(cmd);
    VK_CHECK(vkEndCommandBuffer(cmd));

    VkFenceCreateInfo fenceInfo{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
    VkFence fence = VK_NULL_HANDLE;
    VK_CHECK(vkCreateFence(device_, &fenceInfo, nullptr, &fence));

    VkCommandBufferSubmitInfo cmdInfo{VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO};
    cmdInfo.commandBuffer = cmd;
    VkSubmitInfo2 submit{VK_STRUCTURE_TYPE_SUBMIT_INFO_2};
    submit.commandBufferInfoCount = 1;
    submit.pCommandBufferInfos = &cmdInfo;
    VK_CHECK(vkQueueSubmit2(queue_, 1, &submit, fence));
    VK_CHECK(vkWaitForFences(device_, 1, &fence, VK_TRUE, UINT64_MAX));

    vkDestroyFence(device_, fence, nullptr);
    vkFreeCommandBuffers(device_, uploadPool_, 1, &cmd);
}

} // namespace gfx
