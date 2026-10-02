#include "gfx/SpriteRenderer.h"

#include "gfx/VkCheck.h"
#include "gfx/VulkanContext.h"

#include <SDL3/SDL_log.h>

#include <algorithm>
#include <cstddef>
#include <cstring>

namespace gfx {

namespace {

const uint32_t kSpriteVert[] =
#include "sprite.vert.spv.inc"
    ;
const uint32_t kSpriteFrag[] =
#include "sprite.frag.spv.inc"
    ;

struct PushConstants {
    float screenSize[2];
};

} // namespace

SpriteRenderer::SpriteRenderer(const VulkanContext& ctx, const std::string& atlasPath,
                               VkFormat targetFormat, uint32_t framesInFlight)
    : ctx_(ctx)
{
    if (framesInFlight > kMaxFramesInFlight)
        throw std::runtime_error("SpriteRenderer: too many frames in flight");

    VkDevice device = ctx_.device();
    atlas_ = loadTexture(ctx_, atlasPath);
    sampler_ = createSampler(device, VK_FILTER_NEAREST);
    binding_ = createSamplerBinding(device, atlas_.view, sampler_);

    VkPushConstantRange push{VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(PushConstants)};
    VkPipelineLayoutCreateInfo layoutInfo{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    layoutInfo.setLayoutCount = 1;
    layoutInfo.pSetLayouts = &binding_.layout;
    layoutInfo.pushConstantRangeCount = 1;
    layoutInfo.pPushConstantRanges = &push;
    VK_CHECK(vkCreatePipelineLayout(device, &layoutInfo, nullptr, &layout_));

    VkVertexInputBindingDescription binding{0, sizeof(Sprite), VK_VERTEX_INPUT_RATE_INSTANCE};
    VkVertexInputAttributeDescription attributes[] = {
        {0, 0, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(Sprite, x)},
        {1, 0, VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(Sprite, u)},
        {2, 0, VK_FORMAT_R32_SFLOAT, offsetof(Sprite, rotation)},
        {3, 0, VK_FORMAT_R8G8B8A8_UNORM, offsetof(Sprite, tint)},
    };
    VkPipelineVertexInputStateCreateInfo vertexInput{VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
    vertexInput.vertexBindingDescriptionCount = 1;
    vertexInput.pVertexBindingDescriptions = &binding;
    vertexInput.vertexAttributeDescriptionCount = 4;
    vertexInput.pVertexAttributeDescriptions = attributes;

    GraphicsPipelineDesc desc;
    desc.vertexSpirv = kSpriteVert;
    desc.fragmentSpirv = kSpriteFrag;
    desc.layout = layout_;
    desc.colorFormat = targetFormat;
    desc.alphaBlend = true;
    desc.vertexInput = &vertexInput;
    pipeline_ = createGraphicsPipeline(device, desc);

    for (uint32_t i = 0; i < framesInFlight; ++i)
        instances_[i] = createMappedBuffer(ctx_, sizeof(Sprite) * kMaxSprites, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
}

SpriteRenderer::~SpriteRenderer()
{
    VkDevice device = ctx_.device();
    for (MappedBuffer& buffer : instances_)
        destroyBuffer(ctx_, buffer);
    vkDestroyPipeline(device, pipeline_, nullptr);
    vkDestroyPipelineLayout(device, layout_, nullptr);
    destroySamplerBinding(device, binding_);
    vkDestroySampler(device, sampler_, nullptr);
    destroyImage(ctx_, atlas_);
}

void SpriteRenderer::draw(VkCommandBuffer cmd, uint32_t frame, VkExtent2D screenSize,
                          std::span<const Sprite> sprites)
{
    if (sprites.size() > kMaxSprites && !warnedOverflow_) {
        SDL_LogWarn(SDL_LOG_CATEGORY_RENDER, "%zu sprites submitted; only %u are drawn",
                    sprites.size(), kMaxSprites);
        warnedOverflow_ = true;
    }
    const uint32_t count = static_cast<uint32_t>(std::min<size_t>(sprites.size(), kMaxSprites));
    if (count == 0)
        return;

    MappedBuffer& buffer = instances_[frame];
    std::memcpy(buffer.mapped, sprites.data(), count * sizeof(Sprite));
    VK_CHECK(vmaFlushAllocation(ctx_.allocator(), buffer.allocation, 0, count * sizeof(Sprite)));

    PushConstants push{{float(screenSize.width), float(screenSize.height)}};
    VkDeviceSize offset = 0;

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, layout_, 0, 1, &binding_.set, 0, nullptr);
    vkCmdPushConstants(cmd, layout_, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(push), &push);
    vkCmdBindVertexBuffers(cmd, 0, 1, &buffer.buffer, &offset);
    vkCmdDraw(cmd, 6, count, 0, 0);
}

} // namespace gfx
