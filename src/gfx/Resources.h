#pragma once

#include <vk_mem_alloc.h>
#include <vulkan/vulkan.h>

#include <string>

namespace gfx {

class VulkanContext;

// Host-visible buffer that stays mapped for its whole life.
struct MappedBuffer {
    VkBuffer buffer = VK_NULL_HANDLE;
    VmaAllocation allocation = VK_NULL_HANDLE;
    void* mapped = nullptr;
};

struct Image {
    VkImage image = VK_NULL_HANDLE;
    VmaAllocation allocation = VK_NULL_HANDLE;
    VkImageView view = VK_NULL_HANDLE;
    VkExtent2D extent{};
    VkFormat format = VK_FORMAT_UNDEFINED;
};

MappedBuffer createMappedBuffer(const VulkanContext& ctx, VkDeviceSize size, VkBufferUsageFlags usage);
void destroyBuffer(const VulkanContext& ctx, MappedBuffer& buffer);

Image createImage2D(const VulkanContext& ctx, VkExtent2D extent, VkFormat format, VkImageUsageFlags usage);
void destroyImage(const VulkanContext& ctx, Image& image);

// Loads a PNG as an sRGB texture, left in SHADER_READ_ONLY_OPTIMAL.
Image loadTexture(const VulkanContext& ctx, const std::string& path);

void transitionImage(VkCommandBuffer cmd, VkImage image,
                     VkImageLayout oldLayout, VkImageLayout newLayout,
                     VkPipelineStageFlags2 srcStage, VkAccessFlags2 srcAccess,
                     VkPipelineStageFlags2 dstStage, VkAccessFlags2 dstAccess);

} // namespace gfx
