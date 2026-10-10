#pragma once

#include "gfx/Pipeline.h"
#include "gfx/Resources.h"
#include "gfx/SpriteRenderer.h"
#include "gfx/Starfield.h"
#include "gfx/Swapchain.h"
#include "gfx/VulkanContext.h"

#include <array>
#include <optional>
#include <span>
#include <string>

struct SDL_Window;

namespace gfx {

// Draws the game at the arcade's native 224x256 into an offscreen image, then
// scales that up (whole-number factor where possible) into the window.
class Renderer {
public:
    static constexpr VkExtent2D kVirtualSize{224, 256};

    Renderer(SDL_Window* window, const std::string& atlasPath);
    ~Renderer();

    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;

    // Call when the window size changes; the swapchain is rebuilt before the next frame.
    void onResize() { swapchainDirty_ = true; }

    // Call before Android takes the window's surface away (the app going into the
    // background). Drawing makes a new one once the window has one again.
    void releaseSurface();

    // Where the playfield goes, in window pixels. By default it is centred and
    // scaled by the largest whole number that fits.
    void setGameViewport(float x, float y, float width, float height);

    // The CRT look in the scale-up pass; off gives pixel-exact output.
    void setCrtEnabled(bool enabled) { crtEnabled_ = enabled; }
    bool crtEnabled() const { return crtEnabled_; }

    // Renders and presents one frame: the starfield, then the sprites in order.
    // Sprite coordinates are virtual-screen pixels. `overlay` sprites are drawn
    // last, unscaled, in window pixels.
    void drawFrame(const StarfieldState& stars, std::span<const Sprite> sprites,
                   std::span<const Sprite> overlay = {});

private:
    static constexpr uint32_t kFramesInFlight = 2;
    static constexpr VkFormat kOffscreenFormat = VK_FORMAT_R8G8B8A8_SRGB;

    struct Frame {
        VkCommandPool pool = VK_NULL_HANDLE;
        VkCommandBuffer cmd = VK_NULL_HANDLE;
        VkSemaphore imageAvailable = VK_NULL_HANDLE;
        VkFence inFlight = VK_NULL_HANDLE;
    };

    VkExtent2D windowPixels() const;
    bool recreateSwapchain();
    void createPresentPipeline();
    void record(VkCommandBuffer cmd, uint32_t imageIndex, const StarfieldState& stars,
                std::span<const Sprite> sprites, std::span<const Sprite> overlay);

    SDL_Window* window_;
    std::string atlasPath_;
    VulkanContext ctx_;
    Swapchain swapchain_;
    StarfieldRenderer starfield_;
    SpriteRenderer sprites_;

    Image offscreen_;
    VkSampler presentSampler_ = VK_NULL_HANDLE;
    SamplerBinding presentBinding_;
    VkPipelineLayout presentLayout_ = VK_NULL_HANDLE;
    VkPipeline presentPipeline_ = VK_NULL_HANDLE;
    VkFormat presentFormat_ = VK_FORMAT_UNDEFINED;
    // Sprites drawn straight into the swapchain image, so built for its format.
    std::optional<SpriteRenderer> overlay_;
    std::optional<VkViewport> gameViewport_;

    std::array<Frame, kFramesInFlight> frames_{};
    uint32_t frameIndex_ = 0;
    bool swapchainDirty_ = false;
    bool surfaceLost_ = false;
    bool crtEnabled_ = true;
};

} // namespace gfx
