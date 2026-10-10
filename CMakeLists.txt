cmake_minimum_required(VERSION 3.25)
project(galaxians LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
endif()

if(ANDROID)
    # Built by Gradle (see android/). The NDK has the Vulkan headers and loader
    # and a glslc, and SDL is built from the submodule, whose Java half the APK
    # also uses.
    add_subdirectory(external/SDL EXCLUDE_FROM_ALL)
    find_program(GLSLC glslc REQUIRED
        HINTS ${ANDROID_NDK}/shader-tools/${ANDROID_HOST_TAG} ${ANDROID_NDK}/shader-tools/linux-x86_64)
    find_library(VULKAN_LIBRARY vulkan REQUIRED)
    add_library(Vulkan::Vulkan INTERFACE IMPORTED)
    target_link_libraries(Vulkan::Vulkan INTERFACE ${VULKAN_LIBRARY})
else()
    find_package(Vulkan 1.3 REQUIRED COMPONENTS glslc)
    find_package(SDL3 REQUIRED CONFIG)
    set(GLSLC Vulkan::glslc)
endif()

include(FetchContent)
FetchContent_Declare(VulkanMemoryAllocator
    URL https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator/archive/refs/tags/v3.3.0.tar.gz
    SYSTEM)
FetchContent_MakeAvailable(VulkanMemoryAllocator)

# Shaders are compiled to SPIR-V and embedded in the executable as C arrays.
set(SHADER_SOURCES
    shaders/sprite.vert
    shaders/sprite.frag
    shaders/fullscreen.vert
    shaders/present.frag
    shaders/starfield.frag
)
set(SHADER_OUT_DIR ${CMAKE_BINARY_DIR}/shaders)
file(MAKE_DIRECTORY ${SHADER_OUT_DIR})
foreach(shader ${SHADER_SOURCES})
    get_filename_component(name ${shader} NAME)
    set(output ${SHADER_OUT_DIR}/${name}.spv.inc)
    add_custom_command(
        OUTPUT ${output}
        COMMAND ${GLSLC} --target-env=vulkan1.3 -mfmt=c -o ${output} ${CMAKE_SOURCE_DIR}/${shader}
        DEPENDS ${shader}
        COMMENT "Compiling ${shader}"
        VERBATIM)
    list(APPEND SHADER_OUTPUTS ${output})
endforeach()

set(GAME_SOURCES
    src/main.cpp
    src/gfx/VulkanContext.cpp
    src/gfx/Swapchain.cpp
    src/gfx/Renderer.cpp
    src/gfx/Resources.cpp
    src/gfx/Pipeline.cpp
    src/gfx/SpriteRenderer.cpp
    src/gfx/Starfield.cpp
    src/game/Text.cpp
    src/game/Hud.cpp
    src/game/World.cpp
    src/game/DemoPilot.cpp
    src/game/Game.cpp
    src/audio/Synth.cpp
    src/audio/Audio.cpp
    src/app/Controls.cpp
    src/app/FrameStats.cpp
    src/app/ScreenLayout.cpp
    src/app/TouchControls.cpp
    src/gfx/VmaImpl.cpp
    ${SHADER_OUTPUTS}
)

if(ANDROID)
    # SDLActivity loads the game as libmain.so and calls its SDL_main.
    add_library(galaxians SHARED ${GAME_SOURCES})
    set_target_properties(galaxians PROPERTIES OUTPUT_NAME main)
else()
    add_executable(galaxians ${GAME_SOURCES})
endif()

target_include_directories(galaxians PRIVATE src ${SHADER_OUT_DIR})
target_link_libraries(galaxians PRIVATE Vulkan::Vulkan SDL3::SDL3 GPUOpen::VulkanMemoryAllocator)
target_compile_options(galaxians PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic -Wno-missing-field-initializers>)
# Third-party implementation; its warnings are not ours to fix.
set_source_files_properties(src/gfx/VmaImpl.cpp PROPERTIES COMPILE_OPTIONS -w)

if(ANDROID)
    # The APK carries the assets, and the tests and tools are for the desktop.
    return()
endif()

# Runtime assets live next to the executable.
add_custom_command(TARGET galaxians POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/assets $<TARGET_FILE_DIR:galaxians>/assets)

# Game-logic tests; no GPU or window needed.
enable_testing()
add_executable(world_test
    tests/world_test.cpp
    src/game/World.cpp
    src/game/DemoPilot.cpp
    src/game/Game.cpp
    src/game/Hud.cpp
    src/game/Text.cpp
)
target_include_directories(world_test PRIVATE src)
target_compile_options(world_test PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic -Wno-missing-field-initializers>)
add_test(NAME world_test COMMAND world_test)

add_executable(synth_test
    tests/synth_test.cpp
    src/audio/Synth.cpp
)
target_include_directories(synth_test PRIVATE src)
target_compile_options(synth_test PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic>)
add_test(NAME synth_test COMMAND synth_test)

add_executable(touch_layout_test
    tests/touch_layout_test.cpp
    src/app/ScreenLayout.cpp
)
target_include_directories(touch_layout_test PRIVATE src)
target_compile_options(touch_layout_test PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic>)
add_test(NAME touch_layout_test COMMAND touch_layout_test)

# Renders every sound effect to WAV files for listening: sound_preview <dir>
add_executable(sound_preview
    tools/sound_preview.cpp
    src/audio/Synth.cpp
)
target_include_directories(sound_preview PRIVATE src)
