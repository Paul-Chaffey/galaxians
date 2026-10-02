#pragma once

#include <vk_mem_alloc.h>
#include <vulkan/vulkan.h>

#include <functional>

struct SDL_Window;

namespace gfx {

// Owns the instance, surface, physical/logical device, the queue used for both
// graphics and presentation, and the VMA allocator.
class VulkanContext {
public:
    explicit VulkanContext(SDL_Window* window);
    ~VulkanContext();

    VulkanContext(const VulkanContext&) = delete;
    VulkanContext& operator=(const VulkanContext&) = delete;

    VkInstance instance() const { return instance_; }
    VkSurfaceKHR surface() const { return surface_; }
    VkPhysicalDevice physicalDevice() const { return physicalDevice_; }
    VkDevice device() const { return device_; }
    VkQueue queue() const { return queue_; }
    uint32_t queueFamily() const { return queueFamily_; }
    VmaAllocator allocator() const { return allocator_; }

    // Records commands with `record`, submits them and blocks until the GPU is done.
    // For one-off work such as uploads at load time, not per-frame rendering.
    void immediateSubmit(const std::function<void(VkCommandBuffer)>& record) const;

private:
    void createInstance();
    void createDebugMessenger();
    void pickPhysicalDevice();
    void createDevice();
    void createAllocator();

    VkInstance instance_ = VK_NULL_HANDLE;
    VkDebugUtilsMessengerEXT debugMessenger_ = VK_NULL_HANDLE;
    VkSurfaceKHR surface_ = VK_NULL_HANDLE;
    VkPhysicalDevice physicalDevice_ = VK_NULL_HANDLE;
    VkDevice device_ = VK_NULL_HANDLE;
    VkQueue queue_ = VK_NULL_HANDLE;
    uint32_t queueFamily_ = 0;
    VmaAllocator allocator_ = VK_NULL_HANDLE;
    VkCommandPool uploadPool_ = VK_NULL_HANDLE;
    bool validationEnabled_ = false;
};

} // namespace gfx
