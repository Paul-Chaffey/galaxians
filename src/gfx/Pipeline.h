#pragma once

#include <vulkan/vulkan.h>

#include <span>

namespace gfx {

struct GraphicsPipelineDesc {
    std::span<const uint32_t> vertexSpirv;
    std::span<const uint32_t> fragmentSpirv;
    VkPipelineLayout layout = VK_NULL_HANDLE;
    VkFormat colorFormat = VK_FORMAT_UNDEFINED;
    bool alphaBlend = false;
    // Optional; nullptr means no vertex inputs.
    const VkPipelineVertexInputStateCreateInfo* vertexInput = nullptr;
};

// Triangle-list pipeline for dynamic rendering, with dynamic viewport and scissor.
VkPipeline createGraphicsPipeline(VkDevice device, const GraphicsPipelineDesc& desc);

// Clamped sampler. VK_FILTER_NEAREST keeps pixel art crisp.
VkSampler createSampler(VkDevice device, VkFilter filter);

// A descriptor set holding one combined image sampler at binding 0, visible to
// both vertex and fragment stages, plus the layout and pool that back it.
struct SamplerBinding {
    VkDescriptorSetLayout layout = VK_NULL_HANDLE;
    VkDescriptorPool pool = VK_NULL_HANDLE;
    VkDescriptorSet set = VK_NULL_HANDLE;
};

SamplerBinding createSamplerBinding(VkDevice device, VkImageView view, VkSampler sampler);
void destroySamplerBinding(VkDevice device, SamplerBinding& binding);

} // namespace gfx
