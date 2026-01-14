# Copyright (c) 2025 AIperture-Labs <xavier.beheydt@gmail.com>
# SPDX-License-Identifier: MIT

add_executable(helloworld helloworld.cpp)
enable_sanitizers(helloworld)

# Register the HelloWorld executable as a CTest test
if(ENABLE_TESTING)
    add_test(NAME HelloWorldTest COMMAND helloworld)
endif()

add_executable(Main main.cpp)
set_target_properties(Main PROPERTIES CXX_STANDARD 20)
# target_link_libraries(Main Vulkan::cppm SDL3::SDL3 JPEG::JPEG) if I'm using the legacy libjpeg low level
target_link_libraries(Main Vulkan::cppm SDL3::SDL3 $<IF:$<TARGET_EXISTS:libjpeg-turbo::turbojpeg>,libjpeg-turbo::turbojpeg,libjpeg-turbo::turbojpeg-static>)
if(ENABLE_CPP20_MODULE)
    target_compile_definitions(Main PRIVATE USE_CPP20_MODULES=1)
endif()
if(TRACY_ENABLE)
    target_link_libraries(Main Tracy::TracyClient)
endif()
add_slang_shader_target(
    MainShaders
    SOURCES shader_base.slang
)
add_texture_target(
    MainTextures
    SOURCES texture.jpg
)
add_dependencies(Main MainShaders MainTextures)
