// Copyright (c) 2025 AIperture-Labs <xavier.beheydt@gmail.com>
// SPDX-License-Identifier: MIT

// main.cpp
#if defined(__INTELLISENSE__) || !defined(USE_CPP20_MODULE)
#include <vulkan/vulkan_raii.hpp>
#else
import vulkan_hpp;
#endif

#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>

#include <cstdlib>
#include <iostream>
#include <stdexcept>

/**
 * @class SDLException
 * @brief Exception class for SDL-related errors.
 *
 * This exception is thrown when an SDL function fails. It appends the SDL error message
 * (from SDL_GetError()) to the provided message.
 */
class SDLException : public std::runtime_error {
   public:
    /**
     * @brief Constructs an SDLException with a custom message and the SDL error string.
     * @param message The custom error message.
     */
    explicit SDLException(const std::string &message) : std::runtime_error(message + '\n' + SDL_GetError()) {}
};

class HelloTriangleApplication {
   private:
    static constexpr const char     *window_title  = "Aether Game Engine";
    static constexpr uint16_t        window_width  = 800;
    static constexpr uint16_t        window_height = 600;
    bool                             shouldBeClose = false;
    static constexpr SDL_WindowFlags window_flags =
        SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIDDEN | SDL_WINDOW_HIGH_PIXEL_DENSITY;

    SDL_Window        *window = nullptr;
    vk::raii::Context  context;
    vk::raii::Instance instance = nullptr;

    void initWindow() {
        if (not SDL_Init(SDL_INIT_VIDEO)) throw SDLException("SDL_Init failed");

        window = SDL_CreateWindow(window_title, window_width, window_height, window_flags);
        if (window == nullptr) throw SDLException("SDL_CreateWindow failed");
    }

    void createInstance() {
        constexpr vk::ApplicationInfo appInfo{.pApplicationName   = "Hello Triangle",
                                              .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
                                              .pEngineName        = "Aether Game Engine",
                                              .apiVersion         = vk::ApiVersion14};

        uint32_t sdlExtensionCount = 0;
        auto     sdlExtensions     = SDL_Vulkan_GetInstanceExtensions(&sdlExtensionCount);
        if (sdlExtensions == nullptr) throw SDLException("SDL_Vulkan_GetInstanceExtensions failed");

#if defined(_DEBUG)
        // Retrieve a list of supported extensions
        auto extensions = context.enumerateInstanceExtensionProperties();
        std::cout << "Available extensions:" << std::endl;
        for (const auto &extension : extensions) std::cout << '\t' << extension.extensionName << std::endl;

        std::cout << "SDL Extensions: " << std::endl;
        for (uint8_t i = 0; i < sdlExtensionCount; i++) std::cout << '\t' << sdlExtensions[i] << std::endl;
#endif
        // Check if the required SDL extensions are supported by the Vulkan implementation.
        auto extensionProperties = context.enumerateInstanceExtensionProperties();
        for (uint8_t i = 0; i < sdlExtensionCount; ++i) {
            if (std::ranges::none_of(extensionProperties,
                                     [sdlExtension = sdlExtensions[i]](auto const &extensionProperty) {
                                         return strcmp(extensionProperty.extensionName, sdlExtension) == 0;
                                     })) {
                throw std::runtime_error("Required SDL extension not supported: " + std::string(sdlExtensions[i]));
            }
        }

        vk::InstanceCreateInfo createInfo{.pApplicationInfo        = &appInfo,
                                          .enabledExtensionCount   = sdlExtensionCount,
                                          .ppEnabledExtensionNames = sdlExtensions};


        instance = vk::raii::Instance(context, createInfo);
    }

    void initVulkan() { createInstance(); }

    void mainLoop() {
        SDL_ShowWindow(window);

        while (not shouldBeClose) {
            for (SDL_Event event; SDL_PollEvent(&event);) {
                switch (event.type) {
                    case SDL_EVENT_QUIT:
                        shouldBeClose = true;
                        break;

                    default:
                        break;
                }
            }
        }
    }

    void cleanup() {
        SDL_DestroyWindow(window);
        SDL_Quit();
    }

   public:
    void run() {
        initWindow();
        initVulkan();
        mainLoop();
        cleanup();
    }
};

int main() {
    HelloTriangleApplication app;

    try {
        app.run();
    } catch (const std::exception &e) {
        std::cerr << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}