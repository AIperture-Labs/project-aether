// Copyright (c) 2025 AIperture-Labs <xavier.beheydt@gmail.com>
// SPDX-License-Identifier: MIT

// main.cpp
#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <limits>
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

#if defined(_DEBUG)
#define DEBUG_FUNCTION_LOG() std::cout << "Entry in function: " << __func__ << "();" << std::endl << std::endl;
#else
#define DEBUG_FUNCTION_LOG()
#endif

#if defined(TRACY_ENABLE)

#if defined(__clang__) || defined(__GNUC__)
#define TracyFunction __PRETTY_FUNCTION__
#elif defined(_MSC_VER)
#define TracyFunction __FUNCSIG__
#endif

#include <tracy/Tracy.hpp>

#endif

const std::vector<char const *> validationLayers = {"VK_LAYER_KHRONOS_validation"};

#if defined(_DEBUG)
constexpr bool enableValidationLayers = true;
#else
constexpr bool enableValidationLayers = false;
#endif

constexpr int MAX_FRAMES_IN_FLIGHT = 2;

/**
 * @class SDLException
 * @brief Exception class for SDL-related errors.
 *
 * This exception is thrown when an SDL function fails. It appends the SDL error message
 * (from SDL_GetError()) to the provided message.
 */
class SDLException : public std::runtime_error
{
   public:
    /**
     * @brief Constructs an SDLException with a custom message and the SDL error string.
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
    static constexpr uint16_t        window_width  = 800;
    static constexpr uint16_t        window_height = 600;
    bool                             shouldBeClose = false;
    static constexpr SDL_WindowFlags window_flags =
        SDL_WINDOW_VULKAN | SDL_WINDOW_HIDDEN | SDL_WINDOW_HIGH_PIXEL_DENSITY;

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

    vk::raii::PipelineLayout pipelineLayout   = nullptr;
    vk::raii::Pipeline       graphicsPipeline = nullptr;

    vk::raii::CommandPool                commandPool = nullptr;
    std::vector<vk::raii::CommandBuffer> commandBuffers;

    std::vector<vk::raii::Semaphore> presentCompleteSemaphores;
    std::vector<vk::raii::Semaphore> renderFinishedSemaphores;
    std::vector<vk::raii::Fence>     inFlightFences;
    uint32_t                         frameIndex = 0;

    std::vector<const char *> requiredDeviceExtension = {
        vk::KHRSwapchainExtensionName,
        vk::KHRSpirv14ExtensionName,
        vk::KHRSynchronization2ExtensionName,
        vk::KHRCreateRenderpass2ExtensionName,
    };

    void initWindow()
    {
        DEBUG_FUNCTION_LOG();
        if (not SDL_Init(SDL_INIT_VIDEO))
            throw SDLException("SDL_Init failed");

        window = SDL_CreateWindow(window_title, window_width, window_height, window_flags);
        if (window == nullptr)
            throw SDLException("SDL_CreateWindow failed");
    }

    void initVulkan()
    {
        createInstance();
        setupDebugMessenger();
        createSurface();
        pickPhysicalDevice();
        createLogicalDevice();
        createSwapChain();
        createImagesViews();
        createGraphicsPipeline();
        createCommandPool();
        createCommandBuffers();
        createSyncObjects();
    }

    void mainLoop()
    {
        DEBUG_FUNCTION_LOG();
#if defined(_DEBUG) && defined(TRACY_ENABLE)
        ZoneScoped;
#endif
        bool minimized = false;
        SDL_ShowWindow(window);

        while (not shouldBeClose)
        {
            for (SDL_Event event; SDL_PollEvent(&event);)
            {
                switch (event.type)
                {
                    case SDL_EVENT_QUIT:
                        shouldBeClose = true;
                        break;

                    default:
                        break;
                }
            }
            drawFrame();
        }
        device.waitIdle();
    }

    void cleanup()
    {
        SDL_DestroyWindow(window);
        SDL_Quit();
    }

    void createInstance()
    {
        DEBUG_FUNCTION_LOG();
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

        // Check if the required SDL extensions are supported by the Vulkan implementation.
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
        DEBUG_FUNCTION_LOG();
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
        DEBUG_FUNCTION_LOG();
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
    //     if (devices.empty()) throw std::runtime_error("failed to find GPUs with Vulkan support!");

    //     std::multimap<int, vk::raii::PhysicalDevice> candidates;

    //     for (const auto &device : devices) {
    //         auto     deviceProperties = device.getProperties();
    //         auto     deviceFeatures   = device.getFeatures();
    //         uint32_t score            = 0;

    //         // Discrete GPUs have a significante performance advantage
    //         if (deviceProperties.deviceType == vk::PhysicalDeviceType::eDiscreteGpu) score += 1000;

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
        DEBUG_FUNCTION_LOG();
        std::vector<vk::raii::PhysicalDevice> devices = instance.enumeratePhysicalDevices();
        const auto                            devIter = std::ranges::find_if(devices, [&](auto const &device) {
            // Check if the device supports the Vulkan 1.3 API version
            bool supportsVulkan1_3 = device.getProperties().apiVersion >= VK_API_VERSION_1_3;

            // Check if any of the queue families support graphics operations
            auto queueFamilies    = device.getQueueFamilyProperties();
            bool supportsGraphics = std::ranges::any_of(queueFamilies, [](auto const &qfp) {
                return !!(qfp.queueFlags & vk::QueueFlagBits::eGraphics);
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
    // Example to use two queues if you want to have post-treatment of rendered images.
    // See:
    // https://docs.vulkan.org/tutorial/latest/03_Drawing_a_triangle/01_Presentation/00_Window_surface.html#_creating_the_presentation_queue
    void createLogicalDevice()
    {
        DEBUG_FUNCTION_LOG();
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

    void createSwapChain()
    {
        DEBUG_FUNCTION_LOG();
#if defined(_DEBUG) && defined(TRACY_ENABLE)
        ZoneScoped;
#endif
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

    void createImagesViews()
    {
        DEBUG_FUNCTION_LOG();
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

    void createGraphicsPipeline()
    {
        DEBUG_FUNCTION_LOG();
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

        vk::PipelineVertexInputStateCreateInfo   vertexInputInfo;
        vk::PipelineInputAssemblyStateCreateInfo inputAssembly{.topology = vk::PrimitiveTopology::eTriangleList};
        vk::PipelineViewportStateCreateInfo      viewportState{.viewportCount = 1, .scissorCount = 1};
        vk::PipelineRasterizationStateCreateInfo rasterizer{.depthClampEnable        = vk::False,
                                                            .rasterizerDiscardEnable = vk::False,
                                                            .polygonMode             = vk::PolygonMode::eFill,
                                                            .cullMode                = vk::CullModeFlagBits::eBack,
                                                            .frontFace               = vk::FrontFace::eClockwise,
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

        vk::PipelineLayoutCreateInfo pipelineLayoutInfo{.setLayoutCount = 0, .pushConstantRangeCount = 0};
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
        DEBUG_FUNCTION_LOG();
        vk::CommandPoolCreateInfo poolInfo{.flags            = vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
                                           .queueFamilyIndex = queueIndex};
        commandPool = vk::raii::CommandPool(device, poolInfo);
    }

    void createCommandBuffers()
    {
        DEBUG_FUNCTION_LOG();
        commandBuffers.clear();
        vk::CommandBufferAllocateInfo allocInfo{.commandPool        = commandPool,
                                                .level              = vk::CommandBufferLevel::ePrimary,
                                                .commandBufferCount = MAX_FRAMES_IN_FLIGHT};
        commandBuffers = vk::raii::CommandBuffers(device, allocInfo);
    }

    void recordCommandBuffer(uint32_t imageIndex)
    {
        DEBUG_FUNCTION_LOG();
        auto &commandBuffer = commandBuffers[frameIndex];
        commandBuffer.begin({});

        // Before starting rendering, transition the swapchain image to COLOR_ATTACHMENT_OPTIMAL
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
        commandBuffer.draw(3, 1, 0, 0);
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
        DEBUG_FUNCTION_LOG();
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
        DEBUG_FUNCTION_LOG();
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

    void drawFrame()
    {
        DEBUG_FUNCTION_LOG();
#if defined(_DEBUG) && defined(TRACY_ENABLE)
        ZoneScoped;
#endif
        auto fenceResult = device.waitForFences(*inFlightFences[frameIndex], vk::True, UINT64_MAX);
        if (fenceResult != vk::Result::eSuccess)
        {
            throw std::runtime_error("failed to wait for fence!");
        }
        device.resetFences(*inFlightFences[frameIndex]);

        auto [acquireResult, imageIndex] =
            swapChain.acquireNextImage(UINT64_MAX, *presentCompleteSemaphores[frameIndex], nullptr);

        commandBuffers[frameIndex].reset();
        recordCommandBuffer(imageIndex);

        vk::PipelineStageFlags waitDestinationStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput);
        const vk::SubmitInfo   submitInfo{.waitSemaphoreCount   = 1,
                                          .pWaitSemaphores      = &*presentCompleteSemaphores[frameIndex],
                                          .pWaitDstStageMask    = &waitDestinationStageMask,
                                          .commandBufferCount   = 1,
                                          .pCommandBuffers      = &*commandBuffers[frameIndex],
                                          .signalSemaphoreCount = 1,
                                          .pSignalSemaphores    = &*renderFinishedSemaphores[frameIndex]};
        queue.submit(submitInfo, *inFlightFences[frameIndex]);

        const vk::PresentInfoKHR presentInfoKHR{.waitSemaphoreCount = 1,
                                                .pWaitSemaphores    = &*renderFinishedSemaphores[frameIndex],
                                                .swapchainCount     = 1,
                                                .pSwapchains        = &*swapChain,
                                                .pImageIndices      = &imageIndex};

        vk::Result result = queue.presentKHR(presentInfoKHR);
        switch (result)
        {
            case vk::Result::eSuccess:
                break;
            case vk::Result::eSuboptimalKHR:
                std::cout << "vk::Queue::presentKHR returned vk::Result::eSuboptimalKHR !\n";
                break;
            default:
                break;  // an unexpected result is returned!
        }
        frameIndex = (frameIndex + 1) % MAX_FRAMES_IN_FLIGHT;
        ;
    }

    [[nodiscard]] vk::raii::ShaderModule createShaderModule(const std::vector<char> &code) const
    {
        DEBUG_FUNCTION_LOG();
        vk::ShaderModuleCreateInfo createInfo{.codeSize = code.size() * sizeof(char),
                                              .pCode    = reinterpret_cast<const uint32_t *>(code.data())};
        vk::raii::ShaderModule     shaderModule{device, createInfo};
        return shaderModule;
    }

    static uint32_t calculateMinImageCount(const vk::SurfaceCapabilitiesKHR &surfaceCapabilities)
    {
        DEBUG_FUNCTION_LOG();
        auto minImageCount = std::max(3u, surfaceCapabilities.minImageCount);
        minImageCount = (surfaceCapabilities.maxImageCount > 0 && minImageCount > surfaceCapabilities.maxImageCount)
                            ? surfaceCapabilities.maxImageCount
                            : minImageCount;
        return minImageCount;
    }

    vk::SurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<vk::SurfaceFormatKHR> &availableFormats)
    {
        DEBUG_FUNCTION_LOG();
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
        DEBUG_FUNCTION_LOG();
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
        DEBUG_FUNCTION_LOG();
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
        DEBUG_FUNCTION_LOG();
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
            std::cerr << "validation layer: type " << to_string(type) << " msg: " << pCallbackData->pMessage
                      << std::endl
                      << std::endl;

        return vk::False;
    }

    bool isDeviceSuitable(vk::raii::PhysicalDevice physicalDevice)
    {
        DEBUG_FUNCTION_LOG();
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