#pragma once

#include <vulkan/vulkan.h>

#include <cstdint>

namespace gfx {

class VulkanContext;

// What the game controls about the background starfield.
struct StarfieldState {
    float scroll = 0;        // pixels scrolled; stars move down as this grows
    uint32_t blinkFrame = 0; // advance this to make stars twinkle
};

// Draws the procedural starfield (shaders/starfield.frag) over the whole viewport.
class StarfieldRenderer {
public:
    StarfieldRenderer(const VulkanContext& ctx, VkFormat targetFormat);
    ~StarfieldRenderer();

    StarfieldRenderer(const StarfieldRenderer&) = delete;
    StarfieldRenderer& operator=(const StarfieldRenderer&) = delete;

    // Records the draw into `cmd`, which must be inside a rendering scope.
    void draw(VkCommandBuffer cmd, const StarfieldState& state);

private:
    const VulkanContext& ctx_;
    VkPipelineLayout layout_ = VK_NULL_HANDLE;
    VkPipeline pipeline_ = VK_NULL_HANDLE;
};

} // namespace gfx
