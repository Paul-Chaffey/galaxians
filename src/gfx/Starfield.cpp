#include "gfx/Starfield.h"

#include "gfx/Pipeline.h"
#include "gfx/VkCheck.h"
#include "gfx/VulkanContext.h"

#include <cmath>

namespace gfx {

namespace {

const uint32_t kFullscreenVert[] =
#include "fullscreen.vert.spv.inc"
    ;
const uint32_t kStarfieldFrag[] =
#include "starfield.frag.spv.inc"
    ;

struct PushConstants {
    uint32_t scroll;
    uint32_t blinkFrame;
};

} // namespace

StarfieldRenderer::StarfieldRenderer(const VulkanContext& ctx, VkFormat targetFormat)
    : ctx_(ctx)
{
    VkPushConstantRange push{VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(PushConstants)};
    VkPipelineLayoutCreateInfo layoutInfo{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    layoutInfo.pushConstantRangeCount = 1;
    layoutInfo.pPushConstantRanges = &push;
    VK_CHECK(vkCreatePipelineLayout(ctx_.device(), &layoutInfo, nullptr, &layout_));

    GraphicsPipelineDesc desc;
    desc.vertexSpirv = kFullscreenVert;
    desc.fragmentSpirv = kStarfieldFrag;
    desc.layout = layout_;
    desc.colorFormat = targetFormat;
    pipeline_ = createGraphicsPipeline(ctx_.device(), desc);
}

StarfieldRenderer::~StarfieldRenderer()
{
    vkDestroyPipeline(ctx_.device(), pipeline_, nullptr);
    vkDestroyPipelineLayout(ctx_.device(), layout_, nullptr);
}

void StarfieldRenderer::draw(VkCommandBuffer cmd, const StarfieldState& state)
{
    // Casting through int64 lets the scroll wrap instead of saturating.
    PushConstants push{static_cast<uint32_t>(static_cast<int64_t>(std::floor(state.scroll))),
                       state.blinkFrame};
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_);
    vkCmdPushConstants(cmd, layout_, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(push), &push);
    vkCmdDraw(cmd, 3, 1, 0, 0);
}

} // namespace gfx
