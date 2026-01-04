// Copyright (c) 2025 AIperture-Labs <xavier.beheydt@gmail.com>
// SPDX-License-Identifier: MIT

// main.cpp
#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <map>
#include <stdexcept>
#include <vector>

#if defined(__INTELLISENSE__) || !defined(USE_CPP20_MODULE)
#include <vulkan/vulkan_raii.hpp>
#else
import vulkan_hpp;
#endif

// SDL headers
#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>

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
    vk::raii::PhysicalDevice         physicalDevice = nullptr;
    vk::raii::Device                 device         = nullptr;
    vk::raii::Queue                  graphicsQueue  = nullptr;
    vk::raii::Queue                  presentQueue   = nullptr;
    vk::raii::SurfaceKHR             surface        = nullptr;

    std::vector<const char *> deviceExtensions = {
        vk::KHRSwapchainExtensionName,
        vk::KHRSpirv14ExtensionName,
        vk::KHRSynchronization2ExtensionName,
        vk::KHRCreateRenderpass2ExtensionName,
    };

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
        pickPhysicalDevice();
        createLogicalDevice();
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

    // This is an example how I could design my device selection.
    void examplePickPhysicalDevice() {
        auto devices = instance.enumeratePhysicalDevices();
        if (devices.empty()) throw std::runtime_error("failed to find GPUs with Vulkan support!");

        std::multimap<int, vk::raii::PhysicalDevice> candidates;

        for (const auto &device : devices) {
            auto     deviceProperties = device.getProperties();
            auto     deviceFeatures   = device.getFeatures();
            uint32_t score            = 0;

            // Discrete GPUs have a significante performance advantage
            if (deviceProperties.deviceType == vk::PhysicalDeviceType::eDiscreteGpu) score += 1000;

            // Maximum possible size of textures affects graphics quality
            score += deviceProperties.limits.maxImageDimension2D;

            // Application can't function without geometry shaders
            if (not deviceFeatures.geometryShader) continue;
            candidates.insert(std::make_pair(score, device));
        }

        // Check if the best candidate is suitable at all
        if (candidates.rbegin()->first > 0)
            physicalDevice = candidates.rbegin()->second;
        else
            throw std::runtime_error("failed to find a suitable GPU!");
    }

    // FIXME: It is not clear because the code from tutorial and repo are not the same
    // https://docs.vulkan.org/tutorial/latest/03_Drawing_a_triangle/00_Setup/03_Physical_devices_and_queue_families.html
    // https://github.com/KhronosGroup/Vulkan-Tutorial/blob/main/attachments/05_window_surface.cpp
    void pickPhysicalDevice() {
        std::vector<vk::raii::PhysicalDevice> devices = instance.enumeratePhysicalDevices();
        const auto                            devIter = std::ranges::find_if(devices, [&](auto const &device) {
            auto queueFamilies = device.getQueueFamilyProperties();
            // XXX: Check if Vulkan 1.3 API can be upgraded to 1.4
            bool       isSuitable = device.getProperties().apiVersion >= VK_API_VERSION_1_3;
            const auto qfpIter = std::ranges::find_if(queueFamilies, [](vk::QueueFamilyProperties const &qfp) {
                return (qfp.queueFlags & vk::QueueFlagBits::eGraphics) != static_cast<vk::QueueFlags>(0);
                ;
            });
            isSuitable      = isSuitable && (qfpIter != queueFamilies.end());
            auto extensions = device.enumerateDeviceExtensionProperties();
            bool found      = true;
            for (auto const &extension : deviceExtensions) {
                auto extensionIter = std::ranges::find_if(
                    extensions, [extension](auto const &ext) { return strcmp(ext.extensionName, extension) == 0; });
                found = found && extensionIter != extensions.end();
            }
            isSuitable = isSuitable && found;
            if (isSuitable) physicalDevice = device;
            return isSuitable;
        });
        if (devIter == devices.end()) throw std::runtime_error("failed to find a suitable GPU!");
    }

    // Version generated by devstrall-small-2:24b
    // /**
    //  * @brief Checks if a physical device is suitable for the application's requirements.
    //  *
    //  * This function evaluates whether a Vulkan physical device meets all the criteria needed
    //  * to run the application, including API version, queue families, and required extensions.
    //  *
    //  * @param physicalDevice The Vulkan physical device to evaluate.
    //  * @return true if the device is suitable, false otherwise.
    //  *
    //  * @note This function is designed to be used with Vulkan 1.3+ devices.
    //  *       For Vulkan 1.4 features, additional checks would be needed.
    //  */
    // bool isDeviceSuitable(vk::raii::PhysicalDevice physicalDevice) {
    //     // 1. Check Vulkan API version support (minimum 1.3)
    //     // Vulkan 1.3 introduced important features like dynamic rendering
    //     if (physicalDevice.getProperties().apiVersion < VK_API_VERSION_1_3) {
    //         return false;
    //     }

    //     // 2. Check for graphics queue family support
    //     // We need at least one queue family that supports graphics operations
    //     auto queueFamilies    = physicalDevice.getQueueFamilyProperties();
    //     bool hasGraphicsQueue = std::ranges::any_of(queueFamilies, [](const vk::QueueFamilyProperties &qfp) {
    //         // Check if the queue family supports graphics operations
    //         return (qfp.queueFlags & vk::QueueFlagBits::eGraphics) != vk::QueueFlags(0);
    //     });
    //     if (!hasGraphicsQueue) {
    //         return false;
    //     }

    //     // 3. Check for required device extensions
    //     // These extensions are needed for core functionality like swap chains
    //     auto availableExtensions = physicalDevice.enumerateDeviceExtensionProperties();
    //     for (const auto &requiredExtension : deviceExtensions) {
    //         bool found =
    //             std::ranges::any_of(availableExtensions, [requiredExtension](const vk::ExtensionProperties &ext) {
    //                 // Compare extension names (using C-style string comparison)
    //                 return strcmp(ext.extensionName, requiredExtension) == 0;
    //             });
    //         if (!found) {
    //             return false;
    //         }
    //     }

    //     // If all checks passed, the device is suitable
    //     return true;
    // }
    //
    // void pickPhysicalDevice() {
    //     // Enumerate all available physical devices
    //     auto devices = instance.enumeratePhysicalDevices();

    //     // Find the first suitable device using our selection criteria
    //     const auto devIter =
    //         std::ranges::find_if(devices, [this](const auto &device) { return isDeviceSuitable(device); });

    //     // If no suitable device was found, throw an exception
    //     if (devIter == devices.end()) {
    //         throw std::runtime_error("failed to find a suitable GPU!");
    //     }

    //     // Set the selected device as our physical device
    //     physicalDevice = *devIter;
    // }

    void createLogicalDevice() {
        std::vector<vk::QueueFamilyProperties> queueFamilyProperties = physicalDevice.getQueueFamilyProperties();

        // get the first index into queueFamilyProperties which supports graphics
        auto graphicsQueueFamilyProperty = std::ranges::find_if(queueFamilyProperties, [](auto const &qfp) {
            return (qfp.queueFlags & vk::QueueFlagBits::eGraphics) != static_cast<vk::QueueFlags>(0);
        });
        assert(graphicsQueueFamilyProperty != queueFamilyProperties.end() && "No graphics queue family found!");

        auto graphicsIndex =
            static_cast<uint32_t>(std::distance(queueFamilyProperties.begin(), graphicsQueueFamilyProperty));

        // Create a chain of feature structures
        // XXX: Check if Vulkan 1.3 features can be upgraded to 1.4
        vk::StructureChain<vk::PhysicalDeviceFeatures2,
                           vk::PhysicalDeviceVulkan13Features,
                           vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT>
            featureChain = {
                {},                             // vk::PhysicalDeviceFeatures2 (empty for now)
                {.dynamicRendering = true},     // Enable dynamic rendering from Vulkan 1.3
                {.extendedDynamicState = true}  // Enable extended dynamic state from the extension
            };

        float                     queuePriority = 0.5f;
        vk::DeviceQueueCreateInfo deviceQueueCreateInfo{
            .queueFamilyIndex = graphicsIndex, .queueCount = 1, .pQueuePriorities = &queuePriority};
        vk::DeviceCreateInfo deviceCreateInfo{.pNext                 = &featureChain.get<vk::PhysicalDeviceFeatures2>(),
                                              .queueCreateInfoCount  = 1,
                                              .pQueueCreateInfos     = &deviceQueueCreateInfo,
                                              .enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size()),
                                              .ppEnabledExtensionNames = deviceExtensions.data()};

        device        = vk::raii::Device(physicalDevice, deviceCreateInfo);
        graphicsQueue = vk::raii::Queue(device, graphicsIndex, 0);
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
        // return in Windows
        // - sdlExtensions[0] = VK_KHR_surface
        // - sdlExtensions[2] = VK_KHR_win32_surface
        auto sdlExtensions = SDL_Vulkan_GetInstanceExtensions(&sdlExtensionCount);

        std::vector extensions(sdlExtensions, sdlExtensions + sdlExtensionCount);
        if (enableValidationLayers) extensions.push_back(vk::EXTDebugUtilsExtensionName);

        return extensions;
    }

    static VKAPI_ATTR vk::Bool32 VKAPI_CALL debugCallback(vk::DebugUtilsMessageSeverityFlagBitsEXT      severity,
                                                          vk::DebugUtilsMessageTypeFlagsEXT             type,
                                                          const vk::DebugUtilsMessengerCallbackDataEXT *pCallbackData,
                                                          void *) {
        if (severity == vk::DebugUtilsMessageSeverityFlagBitsEXT::eError ||
            severity == vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning)
            std::cerr << "validation layer: type " << to_string(type) << " msg: " << pCallbackData->pMessage
                      << std::endl;

        return vk::False;
    }

    bool isDeviceSuitable(vk::raii::PhysicalDevice physicalDevice) {
        auto deviceProperties = physicalDevice.getProperties();
        auto deviceFeatures   = physicalDevice.getFeatures();

        return (deviceProperties.deviceType == vk::PhysicalDeviceType::eDiscreteGpu && deviceFeatures.geometryShader);
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