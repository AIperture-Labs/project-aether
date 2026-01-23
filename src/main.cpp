// Copyright (c) 2025 AIperture-Labs <xavier.beheydt@gmail.com>
// SPDX-License-Identifier: MIT

// main.cpp
#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <variant>
#include <vector>

#if defined(__INTELLISENSE__) || !defined(USE_CPP20_MODULE)
#include <vulkan/vulkan_raii.hpp>
#else
import vulkan_hpp;
#endif

// Maths
#define GLM_FORCE_RADIANS
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

// SDL headers
#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>

// Tracy
#if defined(__clang__) || defined(__GNUC__)
#define TracyFunction __PRETTY_FUNCTION__
#elif defined(_MSC_VER)
#define TracyFunction __FUNCSIG__
#endif
#include <tracy/Tracy.hpp>

// TurboJpeg
// FIXME
// #if defined(__clang__) || defined(__GNUC__)
// #define _CRT_SECURE_NO_WARNINGS
// #endif
#include <turbojpeg.h>

const std::vector<char const *> validationLayers = {"VK_LAYER_KHRONOS_validation"};

#if defined(_DEBUG)
constexpr bool enableValidationLayers = true;
#else
constexpr bool enableValidationLayers = false;
#endif

constexpr int MAX_FRAMES_IN_FLIGHT = 2;

struct Vertex
{
    glm::vec2 pos;
    glm::vec3 color;

    static vk::VertexInputBindingDescription getBindingDescription()
    {
        return {0, sizeof(Vertex), vk::VertexInputRate::eVertex};
    }

    static std::array<vk::VertexInputAttributeDescription, 2> getAttributeDescriptions()
    {
        return {vk::VertexInputAttributeDescription(0, 0, vk::Format::eR32G32Sfloat, offsetof(Vertex, pos)),
                vk::VertexInputAttributeDescription(1, 0, vk::Format::eR32G32B32Sfloat, offsetof(Vertex, color))};
    }
};

// Get inspiration from :
//   - Epic UnrealEngine:
//   https://github.com/EpicGames/UnrealEngine/tree/684b4c133ed87e8050d1fdaa287242f0fe2c1153/Engine/Source/Runtime/ImageWrapper
class ImageJpeg
{
    // Members
   public:
    static constexpr int32_t DATA_PRECISION_8_BITS  = 8;
    static constexpr int32_t DATA_PRECISION_12_BITS = 12;
    static constexpr int32_t DATA_PRECISION_16_BITS = 16;

   private:
    tjhandle                   handle;
    const std::string          filename;
    const std::vector<uint8_t> jpegBuf;
    // TODO: later make globals types
    using jpeg_sample_8_t  = uint8_t;
    using jpeg_sample_12_t = int16_t;
    using jpeg_sample_16_t = uint16_t;
    using jpeg_buf_8_t     = std::vector<jpeg_sample_8_t>;
    using jpeg_buf_12_t    = std::vector<jpeg_sample_12_t>;
    using jpeg_buf_16_t    = std::vector<jpeg_sample_16_t>;
    using jpeg_raw_buf_t   = std::variant<jpeg_buf_8_t, jpeg_buf_12_t, jpeg_buf_16_t>;
    jpeg_raw_buf_t rawBuffer;
    const TJPF     pixelFormat;

    // Methods
   public:
    ImageJpeg(const std::string &filename, TJPF pixelFormat = TJPF_RGBA) :
        handle(tj3Init(TJINIT_DECOMPRESS)), filename(filename), jpegBuf(readJpeg(filename)), pixelFormat(pixelFormat)
    {
        if (handle == nullptr)
        {
            throw std::runtime_error(std::string("Failed to initialize TurboJPEG context : ") + tj3GetErrorStr(handle));
        }
        if (tj3DecompressHeader(handle, jpegBuf.data(), getJpegSize()) != 0)
        {
            throw std::runtime_error(std::string("Failed to decompress JPEG header: ") + tj3GetErrorStr(handle));
        }
        if (decompress() != 0)
        {
            throw std::runtime_error(std::string("Failed to decompress JPEG: ") + tj3GetErrorStr(handle));
        }
    }

    // Jpeg(Jpeg &&) = default;
    // Jpeg(const Jpeg &) = default;
    // Jpeg &operator=(Jpeg &&) = default;
    // Jpeg &operator=(const Jpeg &) = default;

    ~ImageJpeg(void)
    {
        tj3Destroy(handle);
    }

    inline int32_t getDataPrecision() const
    {
        return tj3Get(handle, TJPARAM_PRECISION);
    }

    inline size_t getJpegSize() const
    {
        return jpegBuf.size();
    }

    inline uint32_t getWidth() const
    {
        return tj3Get(handle, TJPARAM_JPEGWIDTH);
    }

    inline uint32_t getHeight() const
    {
        return tj3Get(handle, TJPARAM_JPEGHEIGHT);
    }

    inline size_t getPixelSize() const
    {
        return tjPixelSize[pixelFormat];
    }

    inline const void *getData() const
    {
        return std::visit([](auto const &_buf) -> const void * { return _buf.data(); }, rawBuffer);
    }

    inline size_t getSize() const
    {
        return getWidth() * getHeight() * getPixelSize();
    }

    int32_t decompress()
    {
        if (getDataPrecision() == DATA_PRECISION_8_BITS)
        {
            auto &_buf = std::get<jpeg_buf_8_t>(rawBuffer);
            _buf.resize(getSize());
            return tj3Decompress8(handle,
                                  jpegBuf.data(),
                                  getSize(),
                                  _buf.data(),
                                  getWidth() * getPixelSize(),
                                  pixelFormat);
        }
        else if (getDataPrecision() == DATA_PRECISION_12_BITS)
        {
            auto &_buf = std::get<jpeg_buf_12_t>(rawBuffer);
            _buf.resize(getSize());
            return tj3Decompress12(handle,
                                   jpegBuf.data(),
                                   getSize(),
                                   _buf.data(),
                                   getWidth() * getPixelSize(),
                                   pixelFormat);
        }
        else if (getDataPrecision() == DATA_PRECISION_8_BITS)
        {
            auto &_buf = std::get<jpeg_buf_16_t>(rawBuffer);
            _buf.resize(getSize());
            return tj3Decompress16(handle,
                                   jpegBuf.data(),
                                   getSize(),
                                   _buf.data(),
                                   getWidth() * getPixelSize(),
                                   pixelFormat);
        }
        else
        {
            return -1;
        }
    }

   private:
    static std::vector<uint8_t> readJpeg(const std::string &filename)
    {
        std::ifstream file(filename, std::ios::ate | std::ios::binary);

        if (!file.is_open())
        {
            throw std::runtime_error("failed to open file!");
        }

        std::vector<char> buffer(file.tellg());
        file.seekg(0, std::ios::beg);
        file.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
        file.close();

        return std::vector<uint8_t>(buffer.begin(), buffer.end());
    }
};

const std::vector<Vertex> vertices = {{{-0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}},
                                      {{0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}},
                                      {{0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}},
                                      {{-0.5f, 0.5f}, {1.0f, 1.0f, 1.0f}}};

const std::vector<uint16_t> indices = {0, 1, 2, 2, 3, 0};

struct UniformBufferObject
{
    alignas(16) glm::mat4 model;
    alignas(16) glm::mat4 view;
    alignas(16) glm::mat4 proj;
}; /**
    * @class SDLException
    * @brief Exception class for SDL-related errors.
    *
    * This exception is thrown when an SDL function fails. It appends the SDL
    * error message (from SDL_GetError()) to the provided message.
    */
class SDLException : public std::runtime_error
{
   public:
    /**
     * @brief Constructs an SDLException with a custom message and the SDL error
     * string.
     * @param message The custom error message.
     */
    explicit SDLException(const std::string &message) : std::runtime_error(message + '\n' + SDL_GetError())
    {
    }
};

class HelloTriangleApplication
{
   private:
    static constexpr const char     *window_title  = "Aether Game Engine";
    uint16_t                         window_width  = 800;
    uint16_t                         window_height = 600;
    bool                             shouldBeClose = false;
    static constexpr SDL_WindowFlags window_flags =
        SDL_WINDOW_VULKAN | SDL_WINDOW_HIDDEN | SDL_WINDOW_HIGH_PIXEL_DENSITY | SDL_WINDOW_RESIZABLE;

    SDL_Window                      *window = nullptr;
    vk::raii::Context                context;
    vk::raii::Instance               instance       = nullptr;
    vk::raii::DebugUtilsMessengerEXT debugMessenger = nullptr;
    vk::raii::PhysicalDevice         physicalDevice = nullptr;
    vk::raii::Device                 device         = nullptr; /* logicalDevice */
    uint32_t                         queueIndex     = ~0u;
    vk::raii::Queue                  queue          = nullptr;
    vk::raii::SurfaceKHR             surface        = nullptr;
    vk::raii::SwapchainKHR           swapChain      = nullptr;
    std::vector<vk::Image>           swapChainImages;
    vk::SurfaceFormatKHR             swapChainSurfaceFormat;
    vk::Extent2D                     swapChainExtent;
    std::vector<vk::raii::ImageView> swapChainImageViews;

    vk::raii::DescriptorSetLayout descriptorSetLayout = nullptr;
    vk::raii::PipelineLayout      pipelineLayout      = nullptr;
    vk::raii::Pipeline            graphicsPipeline    = nullptr;

    vk::raii::Buffer       vertexBuffer       = nullptr;
    vk::raii::DeviceMemory vertexBufferMemory = nullptr;
    vk::raii::Buffer       indexBuffer        = nullptr;
    vk::raii::DeviceMemory indexBufferMemory  = nullptr;

    std::vector<vk::raii::Buffer>       uniformBuffers;
    std::vector<vk::raii::DeviceMemory> uniformBuffersMemory;
    std::vector<void *>                 uniformBuffersMapped;

    vk::raii::DescriptorPool             descriptorPool = nullptr;
    std::vector<vk::raii::DescriptorSet> descriptorSets;

    vk::raii::CommandPool                commandPool = nullptr;
    std::vector<vk::raii::CommandBuffer> commandBuffers;

    vk::raii::Image        textureImage       = nullptr;
    vk::raii::DeviceMemory textureImageMemory = nullptr;

    std::vector<vk::raii::Semaphore> presentCompleteSemaphores;
    std::vector<vk::raii::Semaphore> renderFinishedSemaphores;
    std::vector<vk::raii::Fence>     inFlightFences;
    uint32_t                         frameIndex = 0;

    bool framebufferResized = false;

    std::vector<const char *> requiredDeviceExtension = {
        vk::KHRSwapchainExtensionName,
        vk::KHRSpirv14ExtensionName,
        vk::KHRSynchronization2ExtensionName,
        vk::KHRCreateRenderpass2ExtensionName,
    };

    void initWindow()
    {
        ZoneScoped;
        if (not SDL_Init(SDL_INIT_VIDEO))
            throw SDLException("SDL_Init failed");

        window = SDL_CreateWindow(window_title, window_width, window_height, window_flags);
        if (window == nullptr)
            throw SDLException("SDL_CreateWindow failed");
    }

    void initVulkan()
    {
        ZoneScoped;
        createInstance();
        setupDebugMessenger();
        createSurface();
        pickPhysicalDevice();
        createLogicalDevice();
        createSwapChain();
        createImageViews();
        createDescriptorSetLayout();
        createGraphicsPipeline();
        createCommandPool();
        createTextureImage();
        createVertexBuffer();
        createIndexBuffer();
        createUniformBuffers();
        createDescriptorPool();
        createDescriptorSets();
        createCommandBuffers();
        createSyncObjects();
    }

    void mainLoop()
    {
        ZoneScoped;
        bool minimized = false;
        SDL_ShowWindow(window);

        while (not shouldBeClose)
        {
            for (SDL_Event event; SDL_PollEvent(&event);)
            {
                if (event.type == SDL_EVENT_QUIT)
                {
                    shouldBeClose = true;
                }
                else if (event.type == SDL_EVENT_WINDOW_MINIMIZED)
                {
                    std::cout << "Window is minimized!" << std::endl;
                    minimized = true;
                }
                else if (event.type == SDL_EVENT_WINDOW_RESTORED)
                {
                    std::cout << "Window is restored!" << std::endl;
                    minimized = false;
                }
            }
            if (not minimized)
            {
                drawFrame();
            }
        }
        device.waitIdle();
        std::cout << "Quitting Hello Triangle Application." << std::endl;
    }

    void cleanup()
    {
        ZoneScoped;
        cleanupSwapChain();

        SDL_DestroyWindow(window);
        SDL_Quit();
    }

    void createInstance()
    {
        ZoneScoped;
        constexpr vk::ApplicationInfo appInfo{.pApplicationName   = "Hello Triangle",
                                              .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
                                              .pEngineName        = "Aether Game Engine",
                                              .apiVersion         = vk::ApiVersion14};

        // Get the required layers
        std::vector<char const *> requiredLayers;
        if (enableValidationLayers)
        {
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

        // Check if the required SDL extensions are supported by the Vulkan
        // implementation.
        auto extensionProperties = context.enumerateInstanceExtensionProperties();
        for (auto const &requiredExtension : requiredExtensions)
        {
            if (std::ranges::none_of(extensionProperties, [requiredExtension](auto const &extensionProperty) {
                    return strcmp(extensionProperty.extensionName, requiredExtension) == 0;
                }))
            {
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

    void setupDebugMessenger()
    {
        if (not enableValidationLayers)
        {
            return;
        }

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
        vk::DebugUtilsMessengerCreateInfoEXT  debugUtilsMessengerCreateInfoEXT{.messageSeverity = severityFlags,
                                                                               .messageType     = messageTypeFlags,
                                                                               .pfnUserCallback = &debugCallback};
        debugMessenger = instance.createDebugUtilsMessengerEXT(debugUtilsMessengerCreateInfoEXT);
    }

    void createSurface()
    {
        ZoneScoped;
        VkSurfaceKHR _surface;
        if (not SDL_Vulkan_CreateSurface(window, *instance, nullptr, &_surface))
        {
            throw std::runtime_error("failed to create window surface!");
        }
        surface = vk::raii::SurfaceKHR(instance, _surface);
    }

    // This is an example how I could design my device selection by the score.
    // void scorePickPhysicalDevice() {
    //     auto devices = instance.enumeratePhysicalDevices();
    //     if (devices.empty()) throw std::runtime_error("failed to find GPUs with
    //     Vulkan support!");

    //     std::multimap<int, vk::raii::PhysicalDevice> candidates;

    //     for (const auto &device : devices) {
    //         auto     deviceProperties = device.getProperties();
    //         auto     deviceFeatures   = device.getFeatures();
    //         uint32_t score            = 0;

    //         // Discrete GPUs have a significante performance advantage
    //         if (deviceProperties.deviceType ==
    //         vk::PhysicalDeviceType::eDiscreteGpu) score += 1000;

    //         // Maximum possible size of textures affects graphics quality
    //         score += deviceProperties.limits.maxImageDimension2D;

    //         // Application can't function without geometry shaders
    //         if (not deviceFeatures.geometryShader) continue;
    //         candidates.insert(std::make_pair(score, device));
    //     }

    //     // Check if the best candidate is suitable at all
    //     if (candidates.rbegin()->first > 0)
    //         physicalDevice = candidates.rbegin()->second;
    //     else
    //         throw std::runtime_error("failed to find a suitable GPU!");
    // }

    void pickPhysicalDevice()
    {
        ZoneScoped;
        std::vector<vk::raii::PhysicalDevice> devices = instance.enumeratePhysicalDevices();
        const auto                            devIter = std::ranges::find_if(devices, [&](auto const &device) {
            // Check if the device supports the Vulkan 1.3 API version
            bool supportsVulkan1_3 = device.getProperties().apiVersion >= VK_API_VERSION_1_3;

            // Check if any of the queue families support graphics operations
            auto queueFamilies    = device.getQueueFamilyProperties();
            bool supportsGraphics = std::ranges::any_of(queueFamilies, [](auto const &qfp) {
                return static_cast<bool>(qfp.queueFlags & vk::QueueFlagBits::eGraphics);
            });

            // Check if all required device extensions are available
            auto availableDeviceExtensions     = device.enumerateDeviceExtensionProperties();
            bool supportsAllRequiredExtensions = std::ranges::all_of(
                requiredDeviceExtension,
                [&availableDeviceExtensions](auto const &requiredDeviceExtension) {
                    return std::ranges::any_of(availableDeviceExtensions,
                                               [requiredDeviceExtension](auto const &availableDeviceExtension) {
                                                   return strcmp(availableDeviceExtension.extensionName,
                                                                 requiredDeviceExtension) == 0;
                                               });
                });

            auto features = device.template getFeatures2<vk::PhysicalDeviceFeatures2,
                                                                                    vk::PhysicalDeviceVulkan11Features,
                                                                                    vk::PhysicalDeviceVulkan13Features,
                                                                                    vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT>();
            bool supportsRequiredFeatures =
                features.template get<vk::PhysicalDeviceVulkan11Features>().shaderDrawParameters &&
                features.template get<vk::PhysicalDeviceVulkan13Features>().synchronization2 &&
                features.template get<vk::PhysicalDeviceVulkan13Features>().dynamicRendering &&
                features.template get<vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT>().extendedDynamicState;

            return supportsVulkan1_3 && supportsGraphics && supportsAllRequiredExtensions && supportsRequiredFeatures;
        });
        if (devIter != devices.end())
        {
            physicalDevice = *devIter;
        }
        else
        {
            throw std::runtime_error("failed to find a suitable GPU!");
        }
    }

    // XXX: if have graphicQueue and presentationQueue separate check👇
    // Example to use two queues if you want to have post-treatment of rendered
    // images. See:
    // https://docs.vulkan.org/tutorial/latest/03_Drawing_a_triangle/01_Presentation/00_Window_surface.html#_creating_the_presentation_queue
    void createLogicalDevice()
    {
        ZoneScoped;
        // TODO: Consider implementing a dedicated transfer queue for buffer
        // operations
        //       This would require:
        //       1. Modifying queue family selection to find a queue with
        //       VK_QUEUE_TRANSFER_BIT
        //       2. Creating a separate command pool for transfer operations
        //       3. Setting resources to VK_SHARING_MODE_CONCURRENT
        //       4. Submitting transfer commands to the transfer queue instead of
        //       graphics queue This is more complex but can improve performance for
        //       large transfers

        // find the index of the first queue family that supports graphics
        std::vector<vk::QueueFamilyProperties> queueFamilyProperties = physicalDevice.getQueueFamilyProperties();

        for (uint32_t qfpIndex = 0; qfpIndex < queueFamilyProperties.size(); qfpIndex++)
        {
            if ((queueFamilyProperties[qfpIndex].queueFlags & vk::QueueFlagBits::eGraphics) &&
                physicalDevice.getSurfaceSupportKHR(qfpIndex, *surface))
            {
                queueIndex = qfpIndex;
                break;
            }
        }
        if (queueIndex == ~0u)
        {
            throw std::runtime_error("Could not find a queue for graphics and present -> terminating");
        }

        // query for Vulkan 1.3 features
        vk::StructureChain<vk::PhysicalDeviceFeatures2,
                           vk::PhysicalDeviceVulkan11Features,
                           vk::PhysicalDeviceVulkan13Features,
                           vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT>
            featureChain = {
                {},                                                            // vk::PhysicalDeviceFeatures2
                {.shaderDrawParameters = vk::True},                            // vk::PhysicalDeviceVulkan11Features
                {.synchronization2 = vk::True, .dynamicRendering = vk::True},  // vk::PhysicalDeviceVulkan13Features
                {.extendedDynamicState = true}  // vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT
            };

        // create a (logical) Device
        float                     queuePriority = 0.5f;
        vk::DeviceQueueCreateInfo deviceQueueCreateInfo{.queueFamilyIndex = queueIndex,
                                                        .queueCount       = 1,
                                                        .pQueuePriorities = &queuePriority};
        vk::DeviceCreateInfo      deviceCreateInfo{
                 .pNext                   = &featureChain.get<vk::PhysicalDeviceFeatures2>(),
                 .queueCreateInfoCount    = 1,
                 .pQueueCreateInfos       = &deviceQueueCreateInfo,
                 .enabledExtensionCount   = static_cast<uint32_t>(requiredDeviceExtension.size()),
                 .ppEnabledExtensionNames = requiredDeviceExtension.data()};

        device = vk::raii::Device(physicalDevice, deviceCreateInfo);
        queue  = vk::raii::Queue(device, queueIndex, 0);
    }

    void cleanupSwapChain()
    {
        ZoneScoped;
        swapChainImageViews.clear();
        swapChain = nullptr;
    }

    void recreateSwapChain()
    {
        ZoneScoped;
        device.waitIdle();

        cleanupSwapChain();

        createSwapChain();
        createImageViews();
    }

    void createSwapChain()
    {
        ZoneScoped;
        auto surfaceCapabilities = physicalDevice.getSurfaceCapabilitiesKHR(surface);
        swapChainSurfaceFormat   = chooseSwapSurfaceFormat(physicalDevice.getSurfaceFormatsKHR(surface));
        swapChainExtent          = chooseSwapExtent(surfaceCapabilities);

        vk::SwapchainCreateInfoKHR swapChainCreateInfo{
            .flags            = vk::SwapchainCreateFlagsKHR(),
            .surface          = surface,
            .minImageCount    = calculateMinImageCount(surfaceCapabilities),
            .imageFormat      = swapChainSurfaceFormat.format,
            .imageColorSpace  = swapChainSurfaceFormat.colorSpace,
            .imageExtent      = swapChainExtent,
            .imageArrayLayers = 1,  // 2 if VR
            .imageUsage       = vk::ImageUsageFlagBits::eColorAttachment,
            .imageSharingMode = vk::SharingMode::eExclusive,
            .preTransform     = surfaceCapabilities.currentTransform,
            .compositeAlpha   = vk::CompositeAlphaFlagBitsKHR::eOpaque,
            .presentMode      = chooseSwapPresentMode(physicalDevice.getSurfacePresentModesKHR(surface)),
            .clipped          = true,
            .oldSwapchain     = nullptr};

        swapChain       = vk::raii::SwapchainKHR(device, swapChainCreateInfo);
        swapChainImages = swapChain.getImages();
    }

    void createImageViews()
    {
        ZoneScoped;
        assert(swapChainImageViews.empty());

        vk::ImageViewCreateInfo imageViewCreateInfo{.viewType         = vk::ImageViewType::e2D,
                                                    .format           = swapChainSurfaceFormat.format,
                                                    .subresourceRange = {.aspectMask = vk::ImageAspectFlagBits::eColor,
                                                                         .baseMipLevel   = 0,
                                                                         .levelCount     = 1,
                                                                         .baseArrayLayer = 0,
                                                                         .layerCount     = 1}};

        for (auto &image : swapChainImages)
        {
            imageViewCreateInfo.image = image;
            swapChainImageViews.emplace_back(device, imageViewCreateInfo);
        }
    }

    void createDescriptorSetLayout()
    {
        vk::DescriptorSetLayoutBinding    uboLayoutBinding(0,
                                                        vk::DescriptorType::eUniformBuffer,
                                                        1,
                                                        vk::ShaderStageFlagBits::eVertex,
                                                        nullptr);
        vk::DescriptorSetLayoutCreateInfo layoutInfo{.bindingCount = 1, .pBindings = &uboLayoutBinding};
        descriptorSetLayout = vk::raii::DescriptorSetLayout(device, layoutInfo);
    }

    void createGraphicsPipeline()
    {
        ZoneScoped;
        std::string filename   = "slang.spv";
        auto        shaderCode = readFile(filename);

#if defined(_DEBUG)
        std::cout << "Shader Buffer Size(" << filename << "): " << shaderCode.size() * sizeof(char) << std::endl;
#endif

        vk::raii::ShaderModule shaderModule = createShaderModule(shaderCode);

        vk::PipelineShaderStageCreateInfo vertShaderStageInfo{.stage  = vk::ShaderStageFlagBits::eVertex,
                                                              .module = shaderModule,
                                                              .pName  = "vertMain"};
        vk::PipelineShaderStageCreateInfo fragShaderStageInfo{.stage  = vk::ShaderStageFlagBits::eFragment,
                                                              .module = shaderModule,
                                                              .pName  = "fragMain"};
        vk::PipelineShaderStageCreateInfo shaderStages[] = {vertShaderStageInfo, fragShaderStageInfo};

        auto                                   bindingDescription   = Vertex::getBindingDescription();
        auto                                   attributeDescription = Vertex::getAttributeDescriptions();
        vk::PipelineVertexInputStateCreateInfo vertexInputInfo{
            .vertexBindingDescriptionCount   = 1,
            .pVertexBindingDescriptions      = &bindingDescription,
            .vertexAttributeDescriptionCount = attributeDescription.size(),
            .pVertexAttributeDescriptions    = attributeDescription.data()};
        vk::PipelineInputAssemblyStateCreateInfo inputAssembly{.topology = vk::PrimitiveTopology::eTriangleList};
        vk::PipelineViewportStateCreateInfo      viewportState{.viewportCount = 1, .scissorCount = 1};
        vk::PipelineRasterizationStateCreateInfo rasterizer{.depthClampEnable        = vk::False,
                                                            .rasterizerDiscardEnable = vk::False,
                                                            .polygonMode             = vk::PolygonMode::eFill,
                                                            .cullMode                = vk::CullModeFlagBits::eBack,
                                                            .frontFace               = vk::FrontFace::eCounterClockwise,
                                                            .depthBiasEnable         = vk::False,
                                                            .depthBiasSlopeFactor    = 1.0f,
                                                            .lineWidth               = 1.0f};
        vk::PipelineMultisampleStateCreateInfo   multisampling{.rasterizationSamples = vk::SampleCountFlagBits::e1,
                                                               .sampleShadingEnable  = vk::False};

        vk::PipelineColorBlendAttachmentState colorBlendAttachement{
            .blendEnable    = vk::False,
            .colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
                              vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA};
        vk::PipelineColorBlendStateCreateInfo colorBlending{.logicOpEnable   = vk::False,
                                                            .logicOp         = vk::LogicOp::eCopy,
                                                            .attachmentCount = 1,
                                                            .pAttachments    = &colorBlendAttachement};

        std::vector dynamicStates = {
            vk::DynamicState::eViewport,
            vk::DynamicState::eScissor,
        };
        vk::PipelineDynamicStateCreateInfo dynamicState{
            .dynamicStateCount = static_cast<uint32_t>(dynamicStates.size()),
            .pDynamicStates    = dynamicStates.data()};

        vk::PipelineLayoutCreateInfo pipelineLayoutInfo{.setLayoutCount         = 1,
                                                        .pSetLayouts            = &*descriptorSetLayout,
                                                        .pushConstantRangeCount = 0};
        pipelineLayout = vk::raii::PipelineLayout(device, pipelineLayoutInfo);

        vk::StructureChain<vk::GraphicsPipelineCreateInfo, vk::PipelineRenderingCreateInfo> pipelineCreateInfoChain = {
            {.stageCount          = 2,
             .pStages             = shaderStages,
             .pVertexInputState   = &vertexInputInfo,
             .pInputAssemblyState = &inputAssembly,
             .pViewportState      = &viewportState,
             .pRasterizationState = &rasterizer,
             .pMultisampleState   = &multisampling,
             .pColorBlendState    = &colorBlending,
             .pDynamicState       = &dynamicState,
             .layout              = *pipelineLayout,
             .renderPass          = nullptr},
            {.colorAttachmentCount = 1, .pColorAttachmentFormats = &swapChainSurfaceFormat.format}};

        graphicsPipeline =
            vk::raii::Pipeline(device, nullptr, pipelineCreateInfoChain.get<vk::GraphicsPipelineCreateInfo>());
    }

    void createCommandPool()
    {
        ZoneScoped;
        vk::CommandPoolCreateInfo poolInfo{.flags            = vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
                                           .queueFamilyIndex = queueIndex};
        commandPool = vk::raii::CommandPool(device, poolInfo);
    }

    void createTextureImage()
    {
        ZoneScoped;
        ImageJpeg img("texture.jpg");

        vk::raii::Buffer       stagingBuffer({});
        vk::raii::DeviceMemory stagingBufferMemory({});
        createBuffer(img.getSize(),
                     vk::BufferUsageFlagBits::eTransferSrc,
                     vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
                     stagingBuffer,
                     stagingBufferMemory);

        void *data = stagingBufferMemory.mapMemory(0, img.getSize());
        memcpy(data, img.getData(), img.getSize());
        stagingBufferMemory.unmapMemory();

        createImage(img.getWidth(),
                    img.getHeight(),
                    vk::Format::eR8G8B8A8Srgb,
                    vk::ImageTiling::eOptimal,
                    vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled,
                    vk::MemoryPropertyFlagBits::eDeviceLocal,
                    textureImage,
                    textureImageMemory);
        transitionImageLayout(textureImage, vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal);
        copyBufferToImage(stagingBuffer, textureImage, img.getWidth(), img.getHeight());
        transitionImageLayout(textureImage,
                              vk::ImageLayout::eTransferDstOptimal,
                              vk::ImageLayout::eShaderReadOnlyOptimal);
    }

    void createImage(uint32_t                width,
                     uint32_t                height,
                     vk::Format              format,
                     vk::ImageTiling         tiling,
                     vk::ImageUsageFlags     usage,
                     vk::MemoryPropertyFlags properties,
                     vk::raii::Image        &image,
                     vk::raii::DeviceMemory &imageMemory)
    {
        vk::ImageCreateInfo imageInfo{.imageType   = vk::ImageType::e2D,
                                      .format      = format,
                                      .extent      = {width, height, 1},
                                      .mipLevels   = 1,
                                      .arrayLayers = 1,
                                      .samples     = vk::SampleCountFlagBits::e1,
                                      .tiling      = tiling,
                                      .usage       = usage,
                                      .sharingMode = vk::SharingMode::eExclusive};

        image = vk::raii::Image(device, imageInfo);

        vk::MemoryRequirements memRequirements = image.getMemoryRequirements();
        vk::MemoryAllocateInfo allocInfo{.allocationSize  = memRequirements.size,
                                         .memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, properties)};
        imageMemory = vk::raii::DeviceMemory(device, allocInfo);
        image.bindMemory(imageMemory, 0);
    }

    vk::raii::CommandBuffer beginSimgleTimeCommands()
    {
        vk::CommandBufferAllocateInfo allocInfo{.commandPool        = commandPool,
                                                .level              = vk::CommandBufferLevel::ePrimary,
                                                .commandBufferCount = 1};
        // Allocate a temporary command buffer for the one-time transfer operation
        // from staging buffer (CPU-accessible) to the final vertex buffer
        // (GPU-local). This buffer is temporary and will be automatically destroyed
        // after the copy operation completes. Unlike the persistent command buffers
        // used for rendering, this buffer is created and used exclusively for data
        // staging and is not reused.
        vk::raii::CommandBuffer commandBuffer = std::move(device.allocateCommandBuffers(allocInfo).front());

        vk::CommandBufferBeginInfo beginInfo{.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit};
        commandBuffer.begin(beginInfo);

        return commandBuffer;
    }

    void endSingleTimeCommands(vk::raii::CommandBuffer &commandBuffer)
    {
        commandBuffer.end();

        vk::SubmitInfo submitInfo{.commandBufferCount = 1, .pCommandBuffers = &*commandBuffer};
        queue.submit(submitInfo, nullptr);
        queue.waitIdle();
    }

    void transitionImageLayout(const vk::raii::Image &image, vk::ImageLayout oldLayout, vk::ImageLayout newLayout)
    {
        auto commandBuffer = beginSimgleTimeCommands();

        vk::ImageMemoryBarrier barrier{.oldLayout        = oldLayout,
                                       .newLayout        = newLayout,
                                       .image            = image,
                                       .subresourceRange = {vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1}};

        vk::PipelineStageFlags sourceStage;
        vk::PipelineStageFlags destinationStage;

        if (oldLayout == vk::ImageLayout::eUndefined && newLayout == vk::ImageLayout::eTransferDstOptimal)
        {
            barrier.srcAccessMask = {};
            barrier.dstAccessMask = vk::AccessFlagBits::eTransferWrite;

            sourceStage      = vk::PipelineStageFlagBits::eTopOfPipe;
            destinationStage = vk::PipelineStageFlagBits::eTransfer;
        }
        else if (oldLayout == vk::ImageLayout::eTransferDstOptimal &&
                 newLayout == vk::ImageLayout::eShaderReadOnlyOptimal)
        {
            barrier.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
            barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;

            sourceStage      = vk::PipelineStageFlagBits::eTransfer;
            destinationStage = vk::PipelineStageFlagBits::eFragmentShader;
        }
        else
        {
            throw std::invalid_argument("unsupported layout transition!");
        }

        commandBuffer.pipelineBarrier(sourceStage, destinationStage, {}, {}, nullptr, barrier);
        endSingleTimeCommands(commandBuffer);
    }

    void copyBufferToImage(const vk::raii::Buffer &buffer, vk::raii::Image &image, uint32_t width, uint32_t height)
    {
        vk::raii::CommandBuffer commandBuffer = beginSimgleTimeCommands();

        vk::BufferImageCopy region{.bufferOffset      = 0,
                                   .bufferRowLength   = 0,
                                   .bufferImageHeight = 0,
                                   .imageSubresource  = {vk::ImageAspectFlagBits::eColor, 0, 0, 1},
                                   .imageOffset       = {0, 0, 0},
                                   .imageExtent       = {width, height, 1}};
        commandBuffer.copyBufferToImage(buffer, image, vk::ImageLayout::eTransferDstOptimal, {region});
        // Submit the buffer copy toe the graphics queue
        endSingleTimeCommands(commandBuffer);
    }

    void createVertexBuffer()
    {
        // TODO: Consider combining vertex and index buffers into a single
        // allocation for better cache locality
        //       and memory efficiency. This follows Vulkan best practices where
        //       multiple buffers are stored in a single VkBuffer with offsets,
        //       making data more cache-friendly and potentially allowing memory
        //       reuse through aliasing when resources aren't used simultaneously.
        //       See:
        //       https://vulkan.lunarg.com/doc/sdk/1.3.280.0/windows/html/vkspec.html#VUID-vkCmdBindVertexBuffers-pVertexBuffers-0x20
        vk::DeviceSize bufferSize = sizeof(vertices[0]) * vertices.size();

        // XXX: Staging buffers are used here to copy data from host-visible memory
        // to device-local memory because many GPUs historically couldn't access
        // system memory directly. On systems with Resizable BAR / Smart Access
        // Memory or UMA, explicit staging may be unnecessary or suboptimal. See
        // discussion:
        // https://www.reddit.com/r/vulkan/comments/1qad9io/continuing_with_the_official_tutorial/
        // Create staging buffer using createBuffer helper
        vk::raii::Buffer       stagingBuffer({});
        vk::raii::DeviceMemory stagingBufferMemory({});
        createBuffer(bufferSize,
                     vk::BufferUsageFlagBits::eTransferSrc,
                     vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
                     stagingBuffer,
                     stagingBufferMemory);

        void *dataStaging = stagingBufferMemory.mapMemory(0, bufferSize);
        memcpy(dataStaging, vertices.data(), bufferSize);
        stagingBufferMemory.unmapMemory();

        // Create vertex buffer using createBuffer helper
        createBuffer(bufferSize,
                     vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eTransferDst,
                     vk::MemoryPropertyFlagBits::eDeviceLocal,
                     vertexBuffer,
                     vertexBufferMemory);

        copyBuffer(stagingBuffer, vertexBuffer, bufferSize);
    }

    void createIndexBuffer()
    {
        vk::DeviceSize bufferSize = sizeof(indices[0]) * indices.size();

        vk::raii::Buffer       stagingBuffer({});
        vk::raii::DeviceMemory stagingBufferMemory({});
        createBuffer(bufferSize,
                     vk::BufferUsageFlagBits::eTransferSrc,
                     vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
                     stagingBuffer,
                     stagingBufferMemory);

        void *data = stagingBufferMemory.mapMemory(0, bufferSize);
        memcpy(data, indices.data(), bufferSize);
        stagingBufferMemory.unmapMemory();

        createBuffer(bufferSize,
                     vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eIndexBuffer,
                     vk::MemoryPropertyFlagBits::eDeviceLocal,
                     indexBuffer,
                     indexBufferMemory);

        copyBuffer(stagingBuffer, indexBuffer, bufferSize);
    }

    void createUniformBuffers()
    {
        uniformBuffers.clear();
        uniformBuffersMemory.clear();
        uniformBuffersMapped.clear();

        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
        {
            vk::DeviceSize         bufferSize = sizeof(UniformBufferObject);
            vk::raii::Buffer       buffer({});
            vk::raii::DeviceMemory bufferMem({});
            createBuffer(bufferSize,
                         vk::BufferUsageFlagBits::eUniformBuffer,
                         vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
                         buffer,
                         bufferMem);
            uniformBuffers.emplace_back(std::move(buffer));
            uniformBuffersMemory.emplace_back(std::move(bufferMem));
            uniformBuffersMapped.emplace_back(uniformBuffersMemory[i].mapMemory(0, bufferSize));
        }
    }

    void createDescriptorPool()
    {
        vk::DescriptorPoolSize       poolSize(vk::DescriptorType::eUniformBuffer, MAX_FRAMES_IN_FLIGHT);
        vk::DescriptorPoolCreateInfo poolInfo{.flags         = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet,
                                              .maxSets       = MAX_FRAMES_IN_FLIGHT,
                                              .poolSizeCount = 1,
                                              .pPoolSizes    = &poolSize};
        descriptorPool = vk::raii::DescriptorPool(device, poolInfo);
    }

    void createDescriptorSets()
    {
        std::vector<vk::DescriptorSetLayout> layouts(MAX_FRAMES_IN_FLIGHT, *descriptorSetLayout);
        vk::DescriptorSetAllocateInfo        allocInfo{.descriptorPool     = descriptorPool,
                                                       .descriptorSetCount = static_cast<uint32_t>(layouts.size()),
                                                       .pSetLayouts        = layouts.data()};

        descriptorSets.clear();
        descriptorSets = device.allocateDescriptorSets(allocInfo);

        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
        {
            vk::DescriptorBufferInfo bufferInfo{.buffer = uniformBuffers[i],
                                                .offset = 0,
                                                .range  = sizeof(UniformBufferObject)};
            vk::WriteDescriptorSet   descriptorWrite{.dstSet          = descriptorSets[i],
                                                     .dstBinding      = 0,
                                                     .dstArrayElement = 0,
                                                     .descriptorCount = 1,
                                                     .descriptorType  = vk::DescriptorType::eUniformBuffer,
                                                     .pBufferInfo     = &bufferInfo};
            device.updateDescriptorSets(descriptorWrite, {});
        }
    }

    void copyBuffer(vk::raii::Buffer &srcBuffer, vk::raii::Buffer &dstBuffer, vk::DeviceSize size)
    {
        vk::raii::CommandBuffer commandCopyBuffer = beginSimgleTimeCommands();
        commandCopyBuffer.copyBuffer(srcBuffer, dstBuffer, vk::BufferCopy(0, 0, size));
        endSingleTimeCommands(commandCopyBuffer);
    }

    void createBuffer(vk::DeviceSize          size,
                      vk::BufferUsageFlags    usage,
                      vk::MemoryPropertyFlags properties,
                      vk::raii::Buffer       &buffer,
                      vk::raii::DeviceMemory &bufferMemory)
    {
        vk::BufferCreateInfo bufferInfo{.size = size, .usage = usage, .sharingMode = vk::SharingMode::eExclusive};
        buffer                                 = vk::raii::Buffer(device, bufferInfo);
        vk::MemoryRequirements memRequirements = buffer.getMemoryRequirements();
        vk::MemoryAllocateInfo allocInfo{.allocationSize  = memRequirements.size,
                                         .memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, properties)};
        bufferMemory = vk::raii::DeviceMemory(device, allocInfo);
        buffer.bindMemory(*bufferMemory, 0);
    }

    uint32_t findMemoryType(uint32_t typeFilter, vk::MemoryPropertyFlags properties)
    {
        vk::PhysicalDeviceMemoryProperties memProperties = physicalDevice.getMemoryProperties();

        for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++)
        {
            if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties)
                return i;
        }

        throw std::runtime_error("failed to find suitable memory type!");
    }

    void createCommandBuffers()
    {
        ZoneScoped;
        commandBuffers.clear();
        vk::CommandBufferAllocateInfo allocInfo{.commandPool        = commandPool,
                                                .level              = vk::CommandBufferLevel::ePrimary,
                                                .commandBufferCount = MAX_FRAMES_IN_FLIGHT};
        commandBuffers = vk::raii::CommandBuffers(device, allocInfo);
    }

    void recordCommandBuffer(uint32_t imageIndex)
    {
        auto &commandBuffer = commandBuffers[frameIndex];
        commandBuffer.begin({});

        // Before starting rendering, transition the swapchain image to
        // COLOR_ATTACHMENT_OPTIMAL
        transition_image_layout(imageIndex,
                                vk::ImageLayout::eUndefined,
                                vk::ImageLayout::eColorAttachmentOptimal,
                                {},  // srcAccessMask (no need to wait for previous operations)
                                vk::AccessFlagBits2::eColorAttachmentWrite,          // dstAccessMask
                                vk::PipelineStageFlagBits2::eColorAttachmentOutput,  // srcStage
                                vk::PipelineStageFlagBits2::eColorAttachmentOutput   // dstStage
        );
        vk::ClearValue              clearColor     = vk::ClearColorValue(0.0f, 0.0f, 0.0f, 1.0f);
        vk::RenderingAttachmentInfo attachmentInfo = {.imageView   = swapChainImageViews[imageIndex],
                                                      .imageLayout = vk::ImageLayout::eColorAttachmentOptimal,
                                                      .loadOp      = vk::AttachmentLoadOp::eClear,
                                                      .storeOp     = vk::AttachmentStoreOp::eStore,
                                                      .clearValue  = clearColor};

        vk::RenderingInfo renderingInfo = {.renderArea = {.offset = {.x = 0, .y = 0}, .extent = swapChainExtent},
                                           .layerCount = 1,
                                           .colorAttachmentCount = 1,
                                           .pColorAttachments    = &attachmentInfo};

        commandBuffer.beginRendering(renderingInfo);
        commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, *graphicsPipeline);
        commandBuffer.setViewport(0,
                                  vk::Viewport(0.0f,
                                               0.0f,
                                               static_cast<float>(swapChainExtent.width),
                                               static_cast<float>(swapChainExtent.height),
                                               0.0f,
                                               1.0f));
        commandBuffer.setScissor(0, vk::Rect2D(vk::Offset2D(0, 0), swapChainExtent));
        commandBuffer.bindVertexBuffers(0, *vertexBuffer, {0});
        commandBuffer.bindIndexBuffer(*indexBuffer, 0, vk::IndexTypeValue<decltype(indices)::value_type>::value);
        commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics,
                                         pipelineLayout,
                                         0,
                                         *descriptorSets[frameIndex],
                                         nullptr);
        commandBuffer.drawIndexed(indices.size(), 1, 0, 0, 0);
        commandBuffer.endRendering();

        // After rendering, transition the swapchain image to PRESENT_SRC
        transition_image_layout(imageIndex,
                                vk::ImageLayout::eColorAttachmentOptimal,
                                vk::ImageLayout::ePresentSrcKHR,
                                vk::AccessFlagBits2::eColorAttachmentWrite,          // srcAccessMask
                                {},                                                  // dstAccessMask
                                vk::PipelineStageFlagBits2::eColorAttachmentOutput,  // srcStage
                                vk::PipelineStageFlagBits2::eBottomOfPipe            // dstStage
        );

        commandBuffer.end();
    }

    void transition_image_layout(uint32_t                imageIndex,
                                 vk::ImageLayout         oldLayout,
                                 vk::ImageLayout         newLayout,
                                 vk::AccessFlags2        srcAccessMask,
                                 vk::AccessFlags2        dstAccessMask,
                                 vk::PipelineStageFlags2 srcStageMask,
                                 vk::PipelineStageFlags2 dstStageMask)
    {
        ZoneScoped;
        vk::ImageMemoryBarrier2 barrier        = {.srcStageMask        = srcStageMask,
                                                  .srcAccessMask       = srcAccessMask,
                                                  .dstStageMask        = dstStageMask,
                                                  .dstAccessMask       = dstAccessMask,
                                                  .oldLayout           = oldLayout,
                                                  .newLayout           = newLayout,
                                                  .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                                                  .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                                                  .image               = swapChainImages[imageIndex],
                                                  .subresourceRange    = {.aspectMask     = vk::ImageAspectFlagBits::eColor,
                                                                          .baseMipLevel   = 0,
                                                                          .levelCount     = 1,
                                                                          .baseArrayLayer = 0,
                                                                          .layerCount     = 1}};
        vk::DependencyInfo      dependencyInfo = {.dependencyFlags         = {},
                                                  .imageMemoryBarrierCount = 1,
                                                  .pImageMemoryBarriers    = &barrier};
        commandBuffers[frameIndex].pipelineBarrier2(dependencyInfo);
    }

    void createSyncObjects()
    {
        ZoneScoped;
        assert(presentCompleteSemaphores.empty() && renderFinishedSemaphores.empty() && inFlightFences.empty());

        for (size_t i = 0; i < swapChainImages.size(); i++)
        {
            renderFinishedSemaphores.emplace_back(device, vk::SemaphoreCreateInfo());
        }

        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
        {
            presentCompleteSemaphores.emplace_back(device, vk::SemaphoreCreateInfo());
            inFlightFences.emplace_back(device, vk::FenceCreateInfo{.flags = vk::FenceCreateFlagBits::eSignaled});
        }
    }

    void updateUniformBuffer(uint32_t currentImage)
    {
        static auto startTime = std::chrono::high_resolution_clock::now();

        auto  currentTime = std::chrono::high_resolution_clock::now();
        float time        = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();

        UniformBufferObject ubo{};
        ubo.model = glm::rotate(glm::mat4(1.0f), time * glm::radians(90.0f), glm::vec3(0.0f, 0.0f, 1.0f));
        ubo.view  = glm::lookAt(glm::vec3(2.0f, 2.0f, 2.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f));
        ubo.proj =
            glm::perspective(glm::radians(45.0f),
                             static_cast<float>(swapChainExtent.width) / static_cast<float>(swapChainExtent.height),
                             0.1f,
                             10.0f);
        // GLM was originally designed for OpenGL, where the Y coordinate of the
        // clip coordinates is inverted. Flip the sign on the Y-axis scaling factor
        // in the projection matrix so the final image isn't rendered upside down.
        ubo.proj[1][1] *= -1;

        memcpy(uniformBuffersMapped[currentImage], &ubo, sizeof(ubo));
    }

    void drawFrame()
    {
        ZoneScoped;
        auto fenceResult = device.waitForFences(*inFlightFences[frameIndex], vk::True, UINT64_MAX);
        if (fenceResult != vk::Result::eSuccess)
        {
            throw std::runtime_error("failed to wait for fence!");
        }
        device.resetFences(*inFlightFences[frameIndex]);

        auto [acquireResult, imageIndex] =
            swapChain.acquireNextImage(UINT64_MAX, *presentCompleteSemaphores[frameIndex], nullptr);

        if (acquireResult == vk::Result::eErrorOutOfDateKHR)
        {
            recreateSwapChain();
            return;
        }

        if (acquireResult != vk::Result::eSuccess && acquireResult != vk::Result::eSuboptimalKHR)
        {
            throw std::runtime_error("failed to acquire swap chain image!");
        }
        updateUniformBuffer(frameIndex);

        device.resetFences(*inFlightFences[frameIndex]);
        commandBuffers[frameIndex].reset();
        recordCommandBuffer(imageIndex);

        vk::PipelineStageFlags waitDestinationStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput);
        const vk::SubmitInfo   submitInfo{.waitSemaphoreCount   = 1,
                                          .pWaitSemaphores      = &*presentCompleteSemaphores[frameIndex],
                                          .pWaitDstStageMask    = &waitDestinationStageMask,
                                          .commandBufferCount   = 1,
                                          .pCommandBuffers      = &*commandBuffers[frameIndex],
                                          .signalSemaphoreCount = 1,
                                          .pSignalSemaphores    = &*renderFinishedSemaphores[imageIndex]};
        queue.submit(submitInfo, *inFlightFences[frameIndex]);

        try
        {
            const vk::PresentInfoKHR presentInfoKHR{.waitSemaphoreCount = 1,
                                                    .pWaitSemaphores    = &*renderFinishedSemaphores[imageIndex],
                                                    .swapchainCount     = 1,
                                                    .pSwapchains        = &*swapChain,
                                                    .pImageIndices      = &imageIndex};
            vk::Result               result = queue.presentKHR(presentInfoKHR);
            if (result == vk::Result::eErrorOutOfDateKHR || result == vk::Result::eSuboptimalKHR || framebufferResized)
            {
                framebufferResized = false;
                recreateSwapChain();
            }
            else if (result != vk::Result::eSuccess)
            {
                throw std::runtime_error("failed to present swap chain image!");
            }
        } catch (const vk::SystemError &e)
        {
            if (e.code().value() == static_cast<int>(vk::Result::eErrorOutOfDateKHR))
            {
                recreateSwapChain();
                return;
            }
            else
            {
                throw;
            }
        }
        frameIndex = (frameIndex + 1) % MAX_FRAMES_IN_FLIGHT;
    }

    [[nodiscard]] vk::raii::ShaderModule createShaderModule(const std::vector<char> &code) const
    {
        ZoneScoped;
        vk::ShaderModuleCreateInfo createInfo{.codeSize = code.size() * sizeof(char),
                                              .pCode    = reinterpret_cast<const uint32_t *>(code.data())};
        vk::raii::ShaderModule     shaderModule{device, createInfo};
        return shaderModule;
    }

    static uint32_t calculateMinImageCount(const vk::SurfaceCapabilitiesKHR &surfaceCapabilities)
    {
        ZoneScoped;
        auto minImageCount = std::max(3u, surfaceCapabilities.minImageCount);
        minImageCount = (surfaceCapabilities.maxImageCount > 0 && minImageCount > surfaceCapabilities.maxImageCount)
                            ? surfaceCapabilities.maxImageCount
                            : minImageCount;
        return minImageCount;
    }

    vk::SurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<vk::SurfaceFormatKHR> &availableFormats)
    {
        ZoneScoped;
        // TODO: switch to ranges any_of ???
        for (const auto &availableFormat : availableFormats)
        {
            if (availableFormat.format == vk::Format::eB8G8R8A8Srgb &&
                availableFormat.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear)
            {
                return availableFormat;
            }
        }
        return availableFormats[0];
    }

    vk::PresentModeKHR chooseSwapPresentMode(const std::vector<vk::PresentModeKHR> &availablePresentModes)
    {
        ZoneScoped;
        // TODO: switch to ranges any_of ???
        for (const auto &availablePresentMode : availablePresentModes)
        {
            if (availablePresentMode == vk::PresentModeKHR::eMailbox)
            {
                return availablePresentMode;
            }
        }
        return vk::PresentModeKHR::eFifo;
    }

    vk::Extent2D chooseSwapExtent(const vk::SurfaceCapabilitiesKHR &capabilities)
    {
        ZoneScoped;
        if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max())
        {
            return capabilities.currentExtent;
        }

        SDL_Surface *sdl_surface = SDL_GetWindowSurface(window);
        if (sdl_surface == nullptr)
        {
            throw std::runtime_error("Failed to get SDL window surface!");
        }

        return {
            std::clamp<uint32_t>(sdl_surface->w, capabilities.minImageExtent.width, capabilities.maxImageExtent.width),
            std::clamp<uint32_t>(sdl_surface->h,
                                 capabilities.minImageExtent.height,
                                 capabilities.maxImageExtent.height)};
    }

    std::vector<const char *> getRequiredExtensions()
    {
        ZoneScoped;
        uint32_t sdlExtensionCount = 0;
        // return in Windows
        // - sdlExtensions[0] = VK_KHR_surface
        // - sdlExtensions[2] = VK_KHR_win32_surface
        auto sdlExtensions = SDL_Vulkan_GetInstanceExtensions(&sdlExtensionCount);

        std::vector extensions(sdlExtensions, sdlExtensions + sdlExtensionCount);
        if (enableValidationLayers)
            extensions.push_back(vk::EXTDebugUtilsExtensionName);

        return extensions;
    }

    static VKAPI_ATTR vk::Bool32 VKAPI_CALL debugCallback(vk::DebugUtilsMessageSeverityFlagBitsEXT      severity,
                                                          vk::DebugUtilsMessageTypeFlagsEXT             type,
                                                          const vk::DebugUtilsMessengerCallbackDataEXT *pCallbackData,
                                                          void *)
    {
        if (severity == vk::DebugUtilsMessageSeverityFlagBitsEXT::eError ||
            severity == vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning)
        {
            std::string message =
                std::format("validation layer: type {} msg: {}\n\n", to_string(type), pCallbackData->pMessage);
            std::cerr << message;
            TracyMessage(message.c_str(), message.size());
        }

        return vk::False;
    }

    bool isDeviceSuitable(vk::raii::PhysicalDevice physicalDevice)
    {
        ZoneScoped;
        auto deviceProperties = physicalDevice.getProperties();
        auto deviceFeatures   = physicalDevice.getFeatures();

        return (deviceProperties.deviceType == vk::PhysicalDeviceType::eDiscreteGpu && deviceFeatures.geometryShader);
    }

    static std::vector<char> readFile(const std::string &filename)
    {
        std::ifstream file(filename, std::ios::ate | std::ios::binary);

        if (!file.is_open())
        {
            throw std::runtime_error("failed to open file!");
        }

        std::vector<char> buffer(file.tellg());
        file.seekg(0, std::ios::beg);
        file.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
        file.close();

        return buffer;
    }

   public:
    void run()
    {
        initWindow();
        initVulkan();
        mainLoop();
        cleanup();
    }
};

int main()
{
#if defined(_DEBUG) && defined(TRACY_ENABLE)
    ZoneScoped;
#endif
    HelloTriangleApplication app;

    try
    {
        app.run();
    } catch (const std::exception &e)
    {
        std::cerr << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}