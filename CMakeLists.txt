# Copyright (c) 2025 AIperture-Labs <xavier.beheydt@gmail.com>

cmake_minimum_required(VERSION 3.20)

# region ==== Settings ====
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

project(
    AetherEngine
    VERSION 0.1.0
)

# Includes
# include(cmake/GitSubmodule.cmake)

# C++ Settings
set(CMAKE_CXX_STANDARD 20 CACHE STRING "C++ standard to use")
set(CMAKE_CXX_STANDARD_REQUIRED ON CACHE BOOL "Require C++ standard")
set(CMAKE_CXX_EXTENSIONS OFF CACHE BOOL "Disable compiler-specific extensions")

set(CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake" ${CMAKE_MODULE_PATH})

option(BUILD_SHARED_LIBS "Enable compilation of shared libraries" OFF)

option(ENABLE_SANITIZERS "Enable Address / Undefined / Thread Sanitizers" OFF)
function(enable_sanitizers TARGET)
    if(NOT ENABLE_SANITIZERS)
        return()
    endif()

    if(WIN32 AND CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
        # Runtime compatible ASan
        set_property(TARGET ${TARGET} PROPERTY
            MSVC_RUNTIME_LIBRARY "MultiThreaded"
        )

        target_compile_options(${TARGET} PRIVATE
            -fsanitize=address,undefined
            -fno-omit-frame-pointer
            -g
        )

        target_link_options(${TARGET} PRIVATE
            -fsanitize=address,undefined
        )

        # 🔑 CRITIQUE : désactiver STL debug
        target_compile_definitions(${TARGET} PRIVATE
            _ITERATOR_DEBUG_LEVEL=0
            _HAS_ITERATOR_DEBUGGING=0
            _DEBUG=0
            NDEBUG
        )
    endif()
endfunction()

option(ENABLE_TESTING "Enable the building of the test" OFF)
if(ENABLE_TESTING)
    include(CTest)
    enable_testing()

    # Coverage
    option(ENABLE_COVERAGE "Enable coverage reporting for gcc/clang" OFF)
    if(ENABLE_COVERAGE)
        message(STATUS "Enable Coverage")
        add_compile_options(--coverage)
        link_libraries(--coverage)
    endif()
endif()


#TODO: option(BUILD_DOC "Build the project's documentation" OFF)

#TODO:
option(ENABLE_STATIC_ANALIZERS "" OFF)
if(ENABLE_STATIC_ANALYZERS)
    # Clang TIDY
    option(ENABLE_CLANG_TIDY "Enable testing with clang-tidy" OFF)
    if(ENABLE_CLANG_TIDY)
        message(STATUS "Enable Clang Tidy")
        find_program(CLANGTIDY clang-tidy)
        if(CLANGTIDY)
            set(CMAKE_CXX_CLANG_TIDY ${CLANGTIDY})
        else()
            message(SEND_ERROR "clang-tidy requested but executable not found")
        endif()
    endif()

    # CPPCheck
    option(ENABLE_CPPCHECK "Enable testing with cppcheck" OFF)
    if(ENABLE_CPPCHECK)
        message(STATUS "Enable CPP Check")
        find_program(CPPCHECK cppcheck)
        if(CPPCHECK)
            set(CMAKE_CXX_CPPCHECK ${CPPCHECK})
        else()
            message(SEND_ERROR "cppcheck requested but executable not found")
        endif()
    endif()
endif()

# Add option to enable/disable C++ 20 module
option(ENABLE_CPP20_MODULE "Enable C++ 20 module support for Vulkan" OFF)

# Enable C++ module dependency scanning only if C++ 20 module is enable
if(ENABLE_CPP20_MODULE)
    set(CMAKE_CXX_SCAN_FOR_MODULES ON)
endif()

find_package(glm CONFIG REQUIRED)
find_package(Vulkan 1.4.335 REQUIRED) # Require Vulkan SDK version 1.4.335 or higher
find_package(SDL3 CONFIG REQUIRED)
# find_package(JPEG REQUIRED) if I'm using the legacy libjpeg low level api
find_package(libjpeg-turbo CONFIG REQUIRED)
if(TRACY_ENABLE)
    find_package(Tracy CONFIG REQUIRED)
endif()

# set up Vulkan C++ module only if enabled
if(ENABLE_CPP20_MODULE)
    add_library(VulkanCppModule)
    add_library(Vulkan::cppm ALIAS VulkanCppModule)

    target_compile_definitions(VulkanCppModule
        PUBLIC VULKAN_HPP_DISPATCH_LOADER_DYNAMIC=1 VULKAN_HPP_NO_STRUCT_CONSTRUCTORS=1
    )
    target_include_directories(VulkanCppModule
        PUBLIC
        "${Vulkan_INCLUDE_DIR}"
    )
    target_link_libraries(VulkanCppModule
        PUBLIC
        Vulkan::Vulkan
    )

    set_target_properties(VulkanCppModule PROPERTIES CXX_STANDARD 20)

    # Add MSVC-specific compiler options for proper C++ module support
    if(MSVC)
        target_compile_options(VulkanCppModule PRIVATE
            /std:c++latest # Use latest C++ standard for better module support
            /permissive- # Standards conformance mode
            /Zc:__cplusplus # Enable correct __cplusplus macro
            /EHsc # Enable C++ exception handling
            /Zc:preprocessor # Use conforming preprocessor
            /translateInclude # Automatically translate #include to import for standard library
        )
    endif()

    target_sources(VulkanCppModule
        PUBLIC
        FILE_SET cxx_modules TYPE CXX_MODULES
        BASE_DIRS
        "${Vulkan_INCLUDE_DIR}"
        FILES
        "${Vulkan_INCLUDE_DIR}/vulkan/vulkan.cppm"
    )


    # Add the vulkan.cppm file directly as a source file
    target_sources(VulkanCppModule
        PRIVATE
        "${Vulkan_INCLUDE_DIR}/vulkan/vulkan.cppm"
    )
else()
    # Create a dummy interface library when C++ 20 module is disabled
    add_library(VulkanCppModule INTERFACE)
    add_library(Vulkan::cppm ALIAS VulkanCppModule)
    target_link_libraries(VulkanCppModule INTERFACE Vulkan::Vulkan)
    target_compile_definitions(VulkanCppModule
        INTERFACE VULKAN_HPP_DISPATCH_LOADER_DYNAMIC=1 VULKAN_HPP_NO_STRUCT_CONSTRUCTORS=1
    )
endif()

# Shaders compilation
add_executable(glslang::validator IMPORTED)
find_program(GLSLANG_VALIDATOR "glslangValidator" HINTS $ENV{VULKAN_SDK}/bin REQUIRED)
set_property(TARGET glslang::validator PROPERTY IMPORTED_LOCATION "${GLSLANG_VALIDATOR}")
find_program(SLANGC_EXECUTABLE slangc HINTS $ENV{VULKAN_SDK}/bin REQUIRED)

function(add_shaders_target TARGET)
    cmake_parse_arguments("SHADER" "" "CHAPTER_NAME" "SOURCES" ${ARGN})
    set(SHADERS_DIR ${SHADER_CHAPTER_NAME}/shaders)
    add_custom_command(
        OUTPUT ${SHADERS_DIR}
        COMMAND ${CMAKE_COMMAND} -E make_directory ${SHADERS_DIR}
    )
    add_custom_command(
        OUTPUT ${SHADERS_DIR}/frag.spv ${SHADERS_DIR}/vert.spv
        COMMAND glslang::validator
        ARGS --target-env vulkan1.0 ${SHADER_SOURCES} --quiet
        WORKING_DIRECTORY ${SHADERS_DIR}
        DEPENDS ${SHADERS_DIR} ${SHADER_SOURCES}
        COMMENT "Compiling Shaders"
        VERBATIM
    )
    add_custom_target(${TARGET} DEPENDS ${SHADERS_DIR}/frag.spv ${SHADERS_DIR}/vert.spv)
endfunction()

# function(add_slang_shader_target TARGET)
#     cmake_parse_arguments("SHADER" "" "CHAPTER_NAME" "SOURCES" ${ARGN})
#     set(SHADERS_DIR ${SHADER_CHAPTER_NAME}/shaders)
#     file(GLOB HAS_COMPUTE ${CHAPTER_SHADER}.comp)
#     set(ENTRY_POINTS -entry vertMain -entry fragMain)
#     if(HAS_COMPUTE)
#         list(APPEND ENTRY_POINTS -entry compMain)
#     endif()
#     add_custom_command(
#         OUTPUT ${SHADERS_DIR}
#         COMMAND ${CMAKE_COMMAND} -E make_directory ${SHADERS_DIR}
#     )
#     add_custom_command(
#         OUTPUT ${SHADERS_DIR}/slang.spv
#         COMMAND ${SLANGC_EXECUTABLE} ${SHADER_SOURCES} -target spirv -profile spirv_1_4+spvRayQueryKHR -emit-spirv-directly -fvk-use-entrypoint-name ${ENTRY_POINTS} -o slang.spv
#         WORKING_DIRECTORY ${SHADERS_DIR}
#         DEPENDS ${SHADERS_DIR} ${SHADER_SOURCES}
#         COMMENT "Compiling Slang Shaders"
#         VERBATIM
#     )
#     add_custom_target(${TARGET} DEPENDS ${SHADERS_DIR}/slang.spv)
# endfunction()

function(add_slang_shader_target TARGET)
    cmake_parse_arguments("SHADER" "" "" "SOURCES" ${ARGN})

    # Use the current source directory relative to the project source
    file(RELATIVE_PATH RELATIVE_SOURCE_DIR
        ${CMAKE_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}
    )

    # Create output directory matching source structure
    set(SHADERS_BINARY_DIR ${CMAKE_BINARY_DIR}/${RELATIVE_SOURCE_DIR})

    set(ENTRY_POINTS -entry vertMain -entry fragMain)
    set(PROFILES -profile spirv_1_4)

    add_custom_command(
        OUTPUT ${SHADERS_BINARY_DIR}/slang.spv
        COMMAND ${CMAKE_COMMAND} -E make_directory ${SHADERS_BINARY_DIR}
        COMMAND ${SLANGC_EXECUTABLE} ${SHADER_SOURCES} -target spirv ${PROFILES} -emit-spirv-directly -fvk-use-entrypoint-name ${ENTRY_POINTS} -o ${SHADERS_BINARY_DIR}/slang.spv
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        DEPENDS ${SHADER_SOURCES}
        COMMENT "Compiling Slang Shaders to ${RELATIVE_SOURCE_DIR}"
        VERBATIM
    )

    add_custom_target(${TARGET} DEPENDS ${SHADERS_BINARY_DIR}/slang.spv)
endfunction()

function(add_texture_target TARGET)
    cmake_parse_arguments("TEXTURE" "" "" "SOURCES" ${ARGN})

    # Use the current source directory relative to the project source
    file(RELATIVE_PATH RELATIVE_SOURCE_DIR
        ${CMAKE_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}
    )

    # Create output directory matching source structure
    set(TEXTURES_BINARY_DIR ${CMAKE_BINARY_DIR}/${RELATIVE_SOURCE_DIR})

    list(APPEND TEXTURE_BINARIES "")

    foreach(TEXTURE_SOURCE ${TEXTURE_SOURCES})
        get_filename_component(TEXTURE_FILENAME "${TEXTURE_SOURCE}" NAME)
        set(TEXTURE_BINARY "${TEXTURES_BINARY_DIR}/${TEXTURE_FILENAME}")
        list(APPEND TEXTURE_BINARIES "${TEXTURE_BINARY}")
        add_custom_command(
            OUTPUT ${TEXTURE_BINARY}
            COMMAND ${CMAKE_COMMAND} -E make_directory ${TEXTURES_BINARY_DIR}
            # FIXME: CMAKE_SOURCE_DIR is not the best way to path the asset
            # COMMAND ${CMAKE_COMMAND} -E create_symlink "${TEXTURE_SOURCE}" ${TEXTURE_BINARY}
            COMMAND ${CMAKE_COMMAND} -E copy_if_different "${TEXTURE_SOURCE}" "${TEXTURE_BINARY}"
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
            DEPENDS ${TEXTURE_SOURCE}
            COMMENT "Symlink ${TEXTURE_SOURCE} to ${TEXTURE_BINARY}"
            VERBATIM
        )
    endforeach()
    add_custom_target(${TARGET} ALL DEPENDS ${TEXTURE_BINARIES})
endfunction()


if(CMAKE_EXPORT_COMPILE_COMMANDS)
    set(CCDB_SRC "${CMAKE_BINARY_DIR}/compile_commands.json")
    set(CCDB_DST "${CMAKE_SOURCE_DIR}/compile_commands.json")

    execute_process(
        COMMAND ${CMAKE_COMMAND} -E create_symlink
        "${CCDB_SRC}"
        "${CCDB_DST}"
        RESULT_VARIABLE SYMLINK_RESULT
        ERROR_QUIET
    )

    if(NOT SYMLINK_RESULT EQUAL 0)
        message(STATUS
            "Symlink for compile_commands.json failed (Windows privileges). "
            "Falling back to copy."
        )

        execute_process(
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CCDB_SRC}"
            "${CCDB_DST}"
        )
    endif()
endif()

if(TRACY_ENABLE)
    set(TRACY_SRC "${CMAKE_BINARY_DIR}/vcpkg_installed/${VCPKG_TARGET_TRIPLET}/tools/tracy")
    set(TRACY_DST "${CMAKE_SOURCE_DIR}/tools/tracy")
    message(${TRACY_SRC})
    execute_process(
        COMMAND ${CMAKE_COMMAND} -E create_symlink
        "${TRACY_SRC}"
        "${TRACY_DST}"
    )
endif()


add_subdirectory(lib)
add_subdirectory(src)
