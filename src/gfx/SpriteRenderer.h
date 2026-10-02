#pragma once

#include "gfx/Pipeline.h"
#include "gfx/Resources.h"
#include "gfx/Sprite.h"

#include <array>
#include <span>
#include <string>

namespace gfx {

class VulkanContext;

// Draws every sprite in a frame with a single instanced draw from one atlas.
class SpriteRenderer {
public:
    static constexpr uint32_t kMaxSprites = 4096;

    SpriteRenderer(const VulkanContext& ctx, const std::string& atlasPath,
                   VkFormat targetFormat, uint32_t framesInFlight);
    ~SpriteRenderer();

    SpriteRenderer(const SpriteRenderer&) = delete;
    SpriteRenderer& operator=(const SpriteRenderer&) = delete;

    // Records the draw into `cmd`, which must be inside a rendering scope whose
    // viewport covers `screenSize` virtual pixels. `frame` selects the instance
    // buffer, so it must not still be in use by the GPU.
    void draw(VkCommandBuffer cmd, uint32_t frame, VkExtent2D screenSize, std::span<const Sprite> sprites);

private:
    static constexpr uint32_t kMaxFramesInFlight = 3;

    const VulkanContext& ctx_;
    Image atlas_;
    VkSampler sampler_ = VK_NULL_HANDLE;
    SamplerBinding binding_;
    VkPipelineLayout layout_ = VK_NULL_HANDLE;
    VkPipeline pipeline_ = VK_NULL_HANDLE;
    std::array<MappedBuffer, kMaxFramesInFlight> instances_{};
    bool warnedOverflow_ = false;
};

} // namespace gfx
