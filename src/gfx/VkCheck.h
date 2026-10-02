#pragma once

#include <vulkan/vulkan.h>

#include <stdexcept>
#include <string>

namespace gfx {

[[noreturn]] inline void vkFail(VkResult result, const char* expr, const char* file, int line)
{
    throw std::runtime_error(std::string(file) + ":" + std::to_string(line) + ": " + expr +
                             " failed with VkResult " + std::to_string(result));
}

} // namespace gfx

#define VK_CHECK(expr)                                                \
    do {                                                              \
        VkResult vkCheckResult_ = (expr);                             \
        if (vkCheckResult_ != VK_SUCCESS)                             \
            ::gfx::vkFail(vkCheckResult_, #expr, __FILE__, __LINE__); \
    } while (0)
