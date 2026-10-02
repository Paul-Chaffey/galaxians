#pragma once

#include <vulkan/vulkan.h>

#include <vector>

namespace gfx {

class VulkanContext;

// Swapchain plus its image views and one "render finished" semaphore per image.
// Present-wait semaphores are per image rather than per frame in flight: an image
// is only reacquired once its previous present has consumed the semaphore.
class Swapchain {
public:
    Swapchain(const VulkanContext& ctx, VkExtent2D windowPixels);
    ~Swapchain();

    Swapchain(const Swapchain&) = delete;
    Swapchain& operator=(const Swapchain&) = delete;

    // Caller must ensure the device is idle.
    void recreate(VkExtent2D windowPixels);

    VkSwapchainKHR handle() const { return swapchain_; }
    VkFormat format() const { return format_; }
    VkExtent2D extent() const { return extent_; }
    VkImage image(uint32_t i) const { return images_[i]; }
    VkImageView view(uint32_t i) const { return views_[i]; }
    VkSemaphore renderFinished(uint32_t i) const { return renderFinished_[i]; }

private:
    void create(VkExtent2D windowPixels, VkSwapchainKHR oldSwapchain);
    void destroyImageResources();

    const VulkanContext& ctx_;
    VkSwapchainKHR swapchain_ = VK_NULL_HANDLE;
    VkFormat format_ = VK_FORMAT_UNDEFINED;
    VkExtent2D extent_{};
    std::vector<VkImage> images_;
    std::vector<VkImageView> views_;
    std::vector<VkSemaphore> renderFinished_;
};

} // namespace gfx
