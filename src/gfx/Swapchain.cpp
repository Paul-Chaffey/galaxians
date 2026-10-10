#include "gfx/Swapchain.h"

#include "gfx/VkCheck.h"
#include "gfx/VulkanContext.h"

#include <algorithm>

namespace gfx {

namespace {

VkSurfaceFormatKHR chooseFormat(VkPhysicalDevice gpu, VkSurfaceKHR surface)
{
    uint32_t count = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(gpu, surface, &count, nullptr);
    std::vector<VkSurfaceFormatKHR> formats(count);
    vkGetPhysicalDeviceSurfaceFormatsKHR(gpu, surface, &count, formats.data());

    for (const auto& f : formats)
        if ((f.format == VK_FORMAT_B8G8R8A8_SRGB || f.format == VK_FORMAT_R8G8B8A8_SRGB) &&
            f.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
            return f;
    return formats.front();
}

} // namespace

Swapchain::Swapchain(const VulkanContext& ctx, VkExtent2D windowPixels)
    : ctx_(ctx)
{
    create(windowPixels, VK_NULL_HANDLE);
}

Swapchain::~Swapchain()
{
    destroyImageResources();
    if (swapchain_)
        vkDestroySwapchainKHR(ctx_.device(), swapchain_, nullptr);
}

void Swapchain::recreate(VkExtent2D windowPixels)
{
    VkSwapchainKHR old = swapchain_;
    destroyImageResources();
    create(windowPixels, old);
    vkDestroySwapchainKHR(ctx_.device(), old, nullptr);
}

void Swapchain::destroy()
{
    destroyImageResources();
    if (swapchain_)
        vkDestroySwapchainKHR(ctx_.device(), swapchain_, nullptr);
    swapchain_ = VK_NULL_HANDLE;
}

void Swapchain::create(VkExtent2D windowPixels, VkSwapchainKHR oldSwapchain)
{
    VkPhysicalDevice gpu = ctx_.physicalDevice();
    VkSurfaceKHR surface = ctx_.surface();
    VkDevice device = ctx_.device();

    VkSurfaceCapabilitiesKHR caps;
    VK_CHECK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(gpu, surface, &caps));

    // Wayland reports 0xFFFFFFFF here and lets the swapchain decide the size.
    if (caps.currentExtent.width != UINT32_MAX) {
        extent_ = caps.currentExtent;
    } else {
        extent_.width = std::clamp(windowPixels.width, caps.minImageExtent.width, caps.maxImageExtent.width);
        extent_.height = std::clamp(windowPixels.height, caps.minImageExtent.height, caps.maxImageExtent.height);
    }

    uint32_t imageCount = caps.minImageCount + 1;
    if (caps.maxImageCount > 0)
        imageCount = std::min(imageCount, caps.maxImageCount);

    VkSurfaceFormatKHR surfaceFormat = chooseFormat(gpu, surface);
    format_ = surfaceFormat.format;

    // TRANSFER_DST lets later milestones blit the low-res game image straight in.
    VkImageUsageFlags usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    if (caps.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_DST_BIT)
        usage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;

    VkSwapchainCreateInfoKHR info{VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR};
    info.surface = surface;
    info.minImageCount = imageCount;
    info.imageFormat = surfaceFormat.format;
    info.imageColorSpace = surfaceFormat.colorSpace;
    info.imageExtent = extent_;
    info.imageArrayLayers = 1;
    info.imageUsage = usage;
    info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    // On a rotated phone, let the compositor turn the image rather than drawing
    // it rotated; this game is cheap enough that the extra pass doesn't matter.
    info.preTransform = (caps.supportedTransforms & VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR)
                            ? VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR
                            : caps.currentTransform;
    rotatedByCompositor_ = info.preTransform != caps.currentTransform;
    info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    info.presentMode = VK_PRESENT_MODE_FIFO_KHR; // vsync; always supported
    info.clipped = VK_TRUE;
    info.oldSwapchain = oldSwapchain;

    VK_CHECK(vkCreateSwapchainKHR(device, &info, nullptr, &swapchain_));

    uint32_t count = 0;
    vkGetSwapchainImagesKHR(device, swapchain_, &count, nullptr);
    images_.resize(count);
    vkGetSwapchainImagesKHR(device, swapchain_, &count, images_.data());

    views_.resize(count);
    renderFinished_.resize(count);
    for (uint32_t i = 0; i < count; ++i) {
        VkImageViewCreateInfo viewInfo{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
        viewInfo.image = images_[i];
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = format_;
        viewInfo.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
        VK_CHECK(vkCreateImageView(device, &viewInfo, nullptr, &views_[i]));

        VkSemaphoreCreateInfo semInfo{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
        VK_CHECK(vkCreateSemaphore(device, &semInfo, nullptr, &renderFinished_[i]));
    }
}

void Swapchain::destroyImageResources()
{
    VkDevice device = ctx_.device();
    for (VkImageView view : views_)
        vkDestroyImageView(device, view, nullptr);
    for (VkSemaphore sem : renderFinished_)
        vkDestroySemaphore(device, sem, nullptr);
    views_.clear();
    renderFinished_.clear();
    images_.clear();
}

} // namespace gfx
