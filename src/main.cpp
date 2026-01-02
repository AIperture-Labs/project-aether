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

const std::vector<char const *> validationLayers = {"VK_LAYER_KHRONOS_validation"};

#if defined(_DEBUG)
constexpr bool enableValidationLayers = true;
#else
constexpr bool enableValidationLayers = false;
#endif

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

    SDL_Window                      *window = nullptr;
    vk::raii::Context                context;
    vk::raii::Instance               instance       = nullptr;
    vk::raii::DebugUtilsMessengerEXT debugMessenger = nullptr;

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

        // Get the required layers
        std::vector<char const *> requiredLayers;
        if (enableValidationLayers) {
            requiredLayers.assign(validationLayers.begin(), validationLayers.end());
        }

        // Check if the required layers are supported by the Vulkan implementation.
        auto layerProperties = context.enumerateInstanceLayerProperties();
        if (std::ranges::any_of(requiredLayers, [&layerProperties](auto const &requiredLayer) {
                return std::ranges::none_of(layerProperties, [requiredLayer](auto const &layerProperty) {
                    return strcmp(layerProperty.layerName, requiredLayer) == 0;
                });
            }))
            throw std::runtime_error("One or more required layers are not supported!");

        // Get the required extensions.
        auto requiredExtensions = getRequiredExtensions();

        // Check if the required SDL extensions are supported by the Vulkan implementation.
        auto extensionProperties = context.enumerateInstanceExtensionProperties();
        for (auto const &requiredExtension : requiredExtensions) {
            if (std::ranges::none_of(extensionProperties, [requiredExtension](auto const &extensionProperty) {
                    return strcmp(extensionProperty.extensionName, requiredExtension) == 0;
                })) {
                throw std::runtime_error("Required extension not supported: " + std::string(requiredExtension));
            }
        }

        vk::InstanceCreateInfo createInfo{.pApplicationInfo        = &appInfo,
                                          .enabledLayerCount       = static_cast<uint32_t>(requiredLayers.size()),
                                          .ppEnabledLayerNames     = requiredLayers.data(),
                                          .enabledExtensionCount   = static_cast<uint32_t>(requiredExtensions.size()),
                                          .ppEnabledExtensionNames = requiredExtensions.data()};

        instance = vk::raii::Instance(context, createInfo);
    }

    void initVulkan() {
        createInstance();
        setupDebugMessenger();
    }

    void setupDebugMessenger() {
        if (not enableValidationLayers) return;

        /*
         There are a lot more settings for the behavior of validation layers than
         just the flags specified in the VkDebugUtilsMessengerCreateInfoEXT
         struct. Browse to the Vulkan SDK and go to the Config directory. There
         you will find a vk_layer_settings.txt file that explains how to
         configure the layers.
        */

        vk::DebugUtilsMessageSeverityFlagsEXT severityFlags(vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose |
                                                            vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning |
                                                            vk::DebugUtilsMessageSeverityFlagBitsEXT::eError);
        vk::DebugUtilsMessageTypeFlagsEXT     messageTypeFlags(vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral |
                                                           vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance |
                                                           vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation);
        vk::DebugUtilsMessengerCreateInfoEXT  debugUtilsMessengerCreateInfoEXT{
             .messageSeverity = severityFlags, .messageType = messageTypeFlags, .pfnUserCallback = &debugCallback};
        debugMessenger = instance.createDebugUtilsMessengerEXT(debugUtilsMessengerCreateInfoEXT);
    }

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

    std::vector<const char *> getRequiredExtensions() {
        uint32_t sdlExtensionCount = 0;
        auto     sdlExtensions     = SDL_Vulkan_GetInstanceExtensions(&sdlExtensionCount);

        std::vector extensions(sdlExtensions, sdlExtensions + sdlExtensionCount);
        if (enableValidationLayers) extensions.push_back(vk::EXTDebugUtilsExtensionName);

        return extensions;
    }

    static VKAPI_ATTR vk::Bool32 VKAPI_CALL debugCallback(vk::DebugUtilsMessageSeverityFlagBitsEXT      severity,
                                                          vk::DebugUtilsMessageTypeFlagsEXT             type,
                                                          const vk::DebugUtilsMessengerCallbackDataEXT *pCallbackData,
                                                          void *) {
        std::cerr << "validation layer: type " << to_string(type) << " msg: " << pCallbackData->pMessage << std::endl;

        return vk::False;
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