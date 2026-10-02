#include "gfx/Renderer.h"

#include "gfx/VkCheck.h"

#include <SDL3/SDL_video.h>

#include <algorithm>
#include <cmath>

namespace gfx {

namespace {

const uint32_t kFullscreenVert[] =
#include "fullscreen.vert.spv.inc"
    ;
const uint32_t kPresentFrag[] =
#include "present.frag.spv.inc"
    ;

struct PresentPushConstants {
    uint32_t crt;
    float scale;
};

// Largest whole-number multiple of `source` that fits `target`, centred. Falls
// back to a fractional fit when the window is smaller than the source.
VkViewport letterbox(VkExtent2D source, VkExtent2D target)
{
    float scale = float(std::min(target.width / source.width, target.height / source.height));
    if (scale < 1.0f)
        scale = std::min(float(target.width) / float(source.width), float(target.height) / float(source.height));

    VkViewport viewport{};
    viewport.width = float(source.width) * scale;
    viewport.height = float(source.height) * scale;
    viewport.x = std::floor((float(target.width) - viewport.width) * 0.5f);
    viewport.y = std::floor((float(target.height) - viewport.height) * 0.5f);
    viewport.maxDepth = 1.0f;
    return viewport;
}

void beginColorRendering(VkCommandBuffer cmd, VkImageView view, VkExtent2D extent, VkClearColorValue clear)
{
    VkRenderingAttachmentInfo color{VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO};
    color.imageView = view;
    color.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    color.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    color.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    color.clearValue.color = clear;

    VkRenderingInfo rendering{VK_STRUCTURE_TYPE_RENDERING_INFO};
    rendering.renderArea = {{0, 0}, extent};
    rendering.layerCount = 1;
    rendering.colorAttachmentCount = 1;
    rendering.pColorAttachments = &color;
    vkCmdBeginRendering(cmd, &rendering);
}

void setViewport(VkCommandBuffer cmd, const VkViewport& viewport, VkExtent2D extent)
{
    VkRect2D scissor{{0, 0}, extent};
    vkCmdSetViewport(cmd, 0, 1, &viewport);
    vkCmdSetScissor(cmd, 0, 1, &scissor);
}

} // namespace

Renderer::Renderer(SDL_Window* window, const std::string& atlasPath)
    : window_(window)
    , ctx_(window)
    , swapchain_(ctx_, windowPixels())
    , starfield_(ctx_, kOffscreenFormat)
    , sprites_(ctx_, atlasPath, kOffscreenFormat, kFramesInFlight)
{
    VkDevice device = ctx_.device();

    offscreen_ = createImage2D(ctx_, kVirtualSize, kOffscreenFormat,
                               VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT);
    // Bilinear for the CRT glow; the shader reads exact texels for the image itself.
    presentSampler_ = createSampler(device, VK_FILTER_LINEAR);
    presentBinding_ = createSamplerBinding(device, offscreen_.view, presentSampler_);

    VkPipelineLayoutCreateInfo layoutInfo{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    layoutInfo.setLayoutCount = 1;
    layoutInfo.pSetLayouts = &presentBinding_.layout;
    VkPushConstantRange push{VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(PresentPushConstants)};
    layoutInfo.pushConstantRangeCount = 1;
    layoutInfo.pPushConstantRanges = &push;
    VK_CHECK(vkCreatePipelineLayout(device, &layoutInfo, nullptr, &presentLayout_));
    createPresentPipeline();

    for (Frame& frame : frames_) {
        VkCommandPoolCreateInfo poolInfo{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
        poolInfo.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
        poolInfo.queueFamilyIndex = ctx_.queueFamily();
        VK_CHECK(vkCreateCommandPool(device, &poolInfo, nullptr, &frame.pool));

        VkCommandBufferAllocateInfo allocInfo{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
        allocInfo.commandPool = frame.pool;
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandBufferCount = 1;
        VK_CHECK(vkAllocateCommandBuffers(device, &allocInfo, &frame.cmd));

        VkSemaphoreCreateInfo semInfo{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
        VK_CHECK(vkCreateSemaphore(device, &semInfo, nullptr, &frame.imageAvailable));

        VkFenceCreateInfo fenceInfo{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
        fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
        VK_CHECK(vkCreateFence(device, &fenceInfo, nullptr, &frame.inFlight));
    }
}

Renderer::~Renderer()
{
    VkDevice device = ctx_.device();
    vkDeviceWaitIdle(device);
    for (Frame& frame : frames_) {
        vkDestroyFence(device, frame.inFlight, nullptr);
        vkDestroySemaphore(device, frame.imageAvailable, nullptr);
        vkDestroyCommandPool(device, frame.pool, nullptr);
    }
    vkDestroyPipeline(device, presentPipeline_, nullptr);
    vkDestroyPipelineLayout(device, presentLayout_, nullptr);
    destroySamplerBinding(device, presentBinding_);
    vkDestroySampler(device, presentSampler_, nullptr);
    destroyImage(ctx_, offscreen_);
}

void Renderer::createPresentPipeline()
{
    if (presentPipeline_)
        vkDestroyPipeline(ctx_.device(), presentPipeline_, nullptr);

    GraphicsPipelineDesc desc;
    desc.vertexSpirv = kFullscreenVert;
    desc.fragmentSpirv = kPresentFrag;
    desc.layout = presentLayout_;
    desc.colorFormat = swapchain_.format();
    presentPipeline_ = createGraphicsPipeline(ctx_.device(), desc);
    presentFormat_ = swapchain_.format();
}

VkExtent2D Renderer::windowPixels() const
{
    int w = 0, h = 0;
    SDL_GetWindowSizeInPixels(window_, &w, &h);
    return {static_cast<uint32_t>(w), static_cast<uint32_t>(h)};
}

bool Renderer::recreateSwapchain()
{
    VkExtent2D size = windowPixels();
    if (size.width == 0 || size.height == 0)
        return false; // minimised; try again later

    vkDeviceWaitIdle(ctx_.device());
    swapchain_.recreate(size);
    if (swapchain_.format() != presentFormat_)
        createPresentPipeline();
    swapchainDirty_ = false;
    return true;
}

void Renderer::drawFrame(const StarfieldState& stars, std::span<const Sprite> sprites)
{
    if (swapchainDirty_ && !recreateSwapchain())
        return;

    VkDevice device = ctx_.device();
    Frame& frame = frames_[frameIndex_];

    VK_CHECK(vkWaitForFences(device, 1, &frame.inFlight, VK_TRUE, UINT64_MAX));

    uint32_t imageIndex = 0;
    VkResult acquire = vkAcquireNextImageKHR(device, swapchain_.handle(), UINT64_MAX,
                                             frame.imageAvailable, VK_NULL_HANDLE, &imageIndex);
    if (acquire == VK_ERROR_OUT_OF_DATE_KHR) {
        swapchainDirty_ = true;
        return;
    }
    if (acquire == VK_SUBOPTIMAL_KHR)
        swapchainDirty_ = true; // still usable; rebuild after presenting
    else if (acquire != VK_SUCCESS)
        vkFail(acquire, "vkAcquireNextImageKHR", __FILE__, __LINE__);

    // Only reset once we know we will submit, otherwise the next wait deadlocks.
    VK_CHECK(vkResetFences(device, 1, &frame.inFlight));
    VK_CHECK(vkResetCommandPool(device, frame.pool, 0));

    record(frame.cmd, imageIndex, stars, sprites);

    VkSemaphore renderFinished = swapchain_.renderFinished(imageIndex);

    VkSemaphoreSubmitInfo waitInfo{VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO};
    waitInfo.semaphore = frame.imageAvailable;
    waitInfo.stageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;

    VkSemaphoreSubmitInfo signalInfo{VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO};
    signalInfo.semaphore = renderFinished;
    signalInfo.stageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;

    VkCommandBufferSubmitInfo cmdInfo{VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO};
    cmdInfo.commandBuffer = frame.cmd;

    VkSubmitInfo2 submit{VK_STRUCTURE_TYPE_SUBMIT_INFO_2};
    submit.waitSemaphoreInfoCount = 1;
    submit.pWaitSemaphoreInfos = &waitInfo;
    submit.commandBufferInfoCount = 1;
    submit.pCommandBufferInfos = &cmdInfo;
    submit.signalSemaphoreInfoCount = 1;
    submit.pSignalSemaphoreInfos = &signalInfo;
    VK_CHECK(vkQueueSubmit2(ctx_.queue(), 1, &submit, frame.inFlight));

    VkSwapchainKHR swapchain = swapchain_.handle();
    VkPresentInfoKHR present{VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};
    present.waitSemaphoreCount = 1;
    present.pWaitSemaphores = &renderFinished;
    present.swapchainCount = 1;
    present.pSwapchains = &swapchain;
    present.pImageIndices = &imageIndex;

    VkResult presented = vkQueuePresentKHR(ctx_.queue(), &present);
    if (presented == VK_ERROR_OUT_OF_DATE_KHR || presented == VK_SUBOPTIMAL_KHR)
        swapchainDirty_ = true;
    else if (presented != VK_SUCCESS)
        vkFail(presented, "vkQueuePresentKHR", __FILE__, __LINE__);

    frameIndex_ = (frameIndex_ + 1) % kFramesInFlight;
}

void Renderer::record(VkCommandBuffer cmd, uint32_t imageIndex,
                      const StarfieldState& stars, std::span<const Sprite> sprites)
{
    VkCommandBufferBeginInfo begin{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    VK_CHECK(vkBeginCommandBuffer(cmd, &begin));

    // Pass 1: starfield and sprites into the native-resolution offscreen image. The previous
    // frame's present pass may still be sampling it, hence the fragment-shader
    // source stage.
    transitionImage(cmd, offscreen_.image,
                    VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                    VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT, VK_ACCESS_2_NONE,
                    VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT);

    beginColorRendering(cmd, offscreen_.view, kVirtualSize, {{0.0f, 0.0f, 0.004f, 1.0f}});
    setViewport(cmd, {0, 0, float(kVirtualSize.width), float(kVirtualSize.height), 0, 1}, kVirtualSize);
    starfield_.draw(cmd, stars);
    sprites_.draw(cmd, frameIndex_, kVirtualSize, sprites);
    vkCmdEndRendering(cmd);

    transitionImage(cmd, offscreen_.image,
                    VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                    VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
                    VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT, VK_ACCESS_2_SHADER_SAMPLED_READ_BIT);

    // Pass 2: scale up into the swapchain image, with black bars around it.
    VkImage image = swapchain_.image(imageIndex);
    VkExtent2D extent = swapchain_.extent();

    transitionImage(cmd, image,
                    VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                    VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_2_NONE,
                    VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT);

    beginColorRendering(cmd, swapchain_.view(imageIndex), extent, {{0.0f, 0.0f, 0.0f, 1.0f}});
    const VkViewport viewport = letterbox(kVirtualSize, extent);
    setViewport(cmd, viewport, extent);
    const PresentPushConstants push{crtEnabled_ ? 1u : 0u, viewport.height / float(kVirtualSize.height)};
    vkCmdPushConstants(cmd, presentLayout_, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(push), &push);
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, presentPipeline_);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, presentLayout_, 0, 1,
                            &presentBinding_.set, 0, nullptr);
    vkCmdDraw(cmd, 3, 1, 0, 0);
    vkCmdEndRendering(cmd);

    // The destination stage must match the stage the render-finished semaphore
    // signals at, so the layout transition completes before present can start.
    transitionImage(cmd, image,
                    VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                    VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
                    VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_2_NONE);

    VK_CHECK(vkEndCommandBuffer(cmd));
}

} // namespace gfx
