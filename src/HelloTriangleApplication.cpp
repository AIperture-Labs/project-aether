#include "HelloTriangleApplication.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <tracy/Tracy.hpp>
#include <vector>

// SDL headers
#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>

// Maths
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

// Vulkan header or module
#if defined(__INTELLISENSE__) || !defined(USE_CPP20_MODULE)
#    include <vulkan/vulkan_raii.hpp>
#else
import vulkan_hpp;
#endif

// Tracy
#if defined(__clang__) || defined(__GNUC__)
#    define TracyFunction __PRETTY_FUNCTION__
#elif defined(_MSC_VER)
#    define TracyFunction __FUNCSIG__
#endif

#include "Geometry/Vextex.hpp"
#include "Images/Jpeg.hpp"
#include "Utils/Handlers.hpp"

void HelloTriangleApplication::initWindow()
{
    ZoneScoped;
    if (not SDL_Init(SDL_INIT_VIDEO))
        throw SDLException("SDL_Init failed");

    window = SDL_CreateWindow(window_title, window_width, window_height, window_flags);
    if (window == nullptr)
        throw SDLException("SDL_CreateWindow failed");
}

void HelloTriangleApplication::initVulkan()
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
    createDepthResources();
    createTextureImage();
    createTextureImageView();
    createTextureSampler();
    createVertexBuffer();
    createIndexBuffer();
    createUniformBuffers();
    createDescriptorPool();
    createDescriptorSets();
    createCommandBuffers();
    createSyncObjects();
}

void HelloTriangleApplication::mainLoop()
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

void HelloTriangleApplication::cleanup()
{
    ZoneScoped;
    cleanupSwapChain();

    SDL_DestroyWindow(window);
    SDL_Quit();
}

void HelloTriangleApplication::createInstance()
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

void HelloTriangleApplication::setupDebugMessenger()
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

void HelloTriangleApplication::createSurface()
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
void HelloTriangleApplication::pickPhysicalDevice()
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
                                                                                // TODO Remove: vk::PhysicalDeviceVulkan11Features,
                                                                                vk::PhysicalDeviceVulkan13Features,
                                                                                vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT>();
        bool supportsRequiredFeatures =
            features.template get<vk::PhysicalDeviceFeatures2>().features.samplerAnisotropy &&
            // TODO Remove: features.template
            // get<vk::PhysicalDeviceVulkan11Features>().shaderDrawParameters &&
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
void HelloTriangleApplication::createLogicalDevice()
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
                       //    vk::PhysicalDeviceVulkan11Features,
                       vk::PhysicalDeviceVulkan13Features,
                       vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT>
        featureChain = {
            {.features = {.samplerAnisotropy = vk::True}},  // vk::PhysicalDeviceFeatures2
            // {.shaderDrawParameters = vk::True},  //
            // vk::PhysicalDeviceVulkan11Features
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

void HelloTriangleApplication::cleanupSwapChain()
{
    ZoneScoped;
    swapChainImageViews.clear();
    swapChain = nullptr;
}

void HelloTriangleApplication::recreateSwapChain()
{
    ZoneScoped;
    device.waitIdle();

    cleanupSwapChain();

    createSwapChain();
    createImageViews();
    createDepthResources();
}

void HelloTriangleApplication::createSwapChain()
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

void HelloTriangleApplication::createImageViews()
{
    ZoneScoped;
    assert(swapChainImageViews.empty());

    vk::ImageViewCreateInfo imageViewCreateInfo{.viewType         = vk::ImageViewType::e2D,
                                                .format           = swapChainSurfaceFormat.format,
                                                .subresourceRange = {.aspectMask     = vk::ImageAspectFlagBits::eColor,
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

void HelloTriangleApplication::createDescriptorSetLayout()
{
    std::array                        bindings = {vk::DescriptorSetLayoutBinding(0,
                                                          vk::DescriptorType::eUniformBuffer,
                                                          1,
                                                          vk::ShaderStageFlagBits::eVertex,
                                                          nullptr),
                                                  vk::DescriptorSetLayoutBinding(1,
                                                          vk::DescriptorType::eCombinedImageSampler,
                                                          1,
                                                          vk::ShaderStageFlagBits::eFragment,
                                                          nullptr)};
    vk::DescriptorSetLayoutCreateInfo layoutInfo{.bindingCount = bindings.size(), .pBindings = bindings.data()};
    descriptorSetLayout = vk::raii::DescriptorSetLayout(device, layoutInfo);
}

void HelloTriangleApplication::createGraphicsPipeline()
{
    ZoneScoped;
    std::string filename   = "slang.spv";
    auto        shaderCode = Utils::Handlers::File::getBuffer<char>(filename);

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

    auto                                   bindingDescription   = Geometry::Vertex::getBindingDescription();
    auto                                   attributeDescription = Geometry::Vertex::getAttributeDescriptions();
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
    vk::PipelineDynamicStateCreateInfo dynamicState{.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size()),
                                                    .pDynamicStates    = dynamicStates.data()};

    vk::PipelineLayoutCreateInfo pipelineLayoutInfo{.setLayoutCount         = 1,
                                                    .pSetLayouts            = &*descriptorSetLayout,
                                                    .pushConstantRangeCount = 0};
    pipelineLayout = vk::raii::PipelineLayout(device, pipelineLayoutInfo);

    vk::PipelineDepthStencilStateCreateInfo depthStencil{.depthTestEnable       = vk::True,
                                                         .depthWriteEnable      = vk::True,
                                                         .depthCompareOp        = vk::CompareOp::eLess,
                                                         .depthBoundsTestEnable = vk::False,
                                                         .stencilTestEnable     = vk::False};

    vk::StructureChain<vk::GraphicsPipelineCreateInfo, vk::PipelineRenderingCreateInfo> pipelineCreateInfoChain = {
        {.stageCount          = 2,
         .pStages             = shaderStages,
         .pVertexInputState   = &vertexInputInfo,
         .pInputAssemblyState = &inputAssembly,
         .pViewportState      = &viewportState,
         .pRasterizationState = &rasterizer,
         .pMultisampleState   = &multisampling,
         .pDepthStencilState  = &depthStencil,
         .pColorBlendState    = &colorBlending,
         .pDynamicState       = &dynamicState,
         .layout              = *pipelineLayout,
         .renderPass          = nullptr},
        {.colorAttachmentCount    = 1,
         .pColorAttachmentFormats = &swapChainSurfaceFormat.format,
         .depthAttachmentFormat   = findDepthFormat()}};

    graphicsPipeline =
        vk::raii::Pipeline(device, nullptr, pipelineCreateInfoChain.get<vk::GraphicsPipelineCreateInfo>());
}

void HelloTriangleApplication::createCommandPool()
{
    ZoneScoped;
    vk::CommandPoolCreateInfo poolInfo{.flags            = vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
                                       .queueFamilyIndex = queueIndex};
    commandPool = vk::raii::CommandPool(device, poolInfo);
}

void HelloTriangleApplication::createDepthResources()
{
    vk::Format depthFormat = findDepthFormat();
    createImage(swapChainExtent.width,
                swapChainExtent.height,
                depthFormat,
                vk::ImageTiling::eOptimal,
                vk::ImageUsageFlagBits::eDepthStencilAttachment,
                vk::MemoryPropertyFlagBits::eDeviceLocal,
                depthImage,
                depthImageMemory);
    depthImageView = createImageView(depthImage, depthFormat, vk::ImageAspectFlagBits::eDepth);
}

vk::Format HelloTriangleApplication::findSupportedFormat(const std::vector<vk::Format> &candidates,
                                                         vk::ImageTiling                tiling,
                                                         vk::FormatFeatureFlags         features)
{
    for (const auto format : candidates)
    {
        vk::FormatProperties props = physicalDevice.getFormatProperties(format);

        if (tiling == vk::ImageTiling::eLinear && (props.linearTilingFeatures & features) == features)
        {
            return format;
        }
        if (tiling == vk::ImageTiling::eOptimal && (props.optimalTilingFeatures & features) == features)
        {
            return format;
        }
    }

    throw std::runtime_error("failed to find supported format!");
}

vk::Format HelloTriangleApplication::findDepthFormat()
{
    return findSupportedFormat({vk::Format::eD32Sfloat, vk::Format::eD32SfloatS8Uint, vk::Format::eD24UnormS8Uint},
                               vk::ImageTiling::eOptimal,
                               vk::FormatFeatureFlagBits::eDepthStencilAttachment);
}

bool HelloTriangleApplication::hasStencileComponent(vk::Format format)
{
    return format == vk::Format::eD32SfloatS8Uint || format == vk::Format::eD24UnormS8Uint;
}

void HelloTriangleApplication::createTextureImage()
{
    ZoneScoped;
    Images::Jpeg img("texture.jpg");

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
    transitionImageLayout(textureImage, vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eShaderReadOnlyOptimal);
}

void HelloTriangleApplication::createImage(uint32_t                width,
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

vk::raii::CommandBuffer HelloTriangleApplication::beginSimgleTimeCommands()
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

void HelloTriangleApplication::endSingleTimeCommands(vk::raii::CommandBuffer &commandBuffer)
{
    commandBuffer.end();

    vk::SubmitInfo submitInfo{.commandBufferCount = 1, .pCommandBuffers = &*commandBuffer};
    queue.submit(submitInfo, nullptr);
    queue.waitIdle();
}

void HelloTriangleApplication::transitionImageLayout(const vk::raii::Image &image,
                                                     vk::ImageLayout        oldLayout,
                                                     vk::ImageLayout        newLayout)
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
    else if (oldLayout == vk::ImageLayout::eTransferDstOptimal && newLayout == vk::ImageLayout::eShaderReadOnlyOptimal)
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

void HelloTriangleApplication::copyBufferToImage(const vk::raii::Buffer &buffer,
                                                 vk::raii::Image        &image,
                                                 uint32_t                width,
                                                 uint32_t                height)
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

void HelloTriangleApplication::createTextureImageView()
{
    textureImageView = createImageView(textureImage, vk::Format::eR8G8B8A8Srgb, vk::ImageAspectFlagBits::eColor);
}

vk::raii::ImageView HelloTriangleApplication::createImageView(vk::raii::Image     &image,
                                                              vk::Format           format,
                                                              vk::ImageAspectFlags aspectFlags)
{
    vk::ImageViewCreateInfo viewInfo{.image            = image,
                                     .viewType         = vk::ImageViewType::e2D,
                                     .format           = format,
                                     .subresourceRange = {.aspectMask     = aspectFlags,
                                                          .baseMipLevel   = 0,
                                                          .levelCount     = 1,
                                                          .baseArrayLayer = 0,
                                                          .layerCount     = 1}};
    return vk::raii::ImageView(device, viewInfo);
}

void HelloTriangleApplication::createTextureSampler()
{
    vk::PhysicalDeviceProperties properties = physicalDevice.getProperties();
    vk::SamplerCreateInfo        samplerInfo{.magFilter        = vk::Filter::eLinear,
                                             .minFilter        = vk::Filter::eLinear,
                                             .mipmapMode       = vk::SamplerMipmapMode::eLinear,
                                             .addressModeU     = vk::SamplerAddressMode::eRepeat,
                                             .addressModeV     = vk::SamplerAddressMode::eRepeat,
                                             .addressModeW     = vk::SamplerAddressMode::eRepeat,
                                             .anisotropyEnable = vk::True,
                                             .maxAnisotropy    = properties.limits.maxSamplerAnisotropy,
                                             .compareEnable    = vk::False,
                                             .compareOp        = vk::CompareOp::eAlways};
    textureSampler = vk::raii::Sampler(device, samplerInfo);
}

void HelloTriangleApplication::createVertexBuffer()
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

void HelloTriangleApplication::createIndexBuffer()
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

void HelloTriangleApplication::createUniformBuffers()
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

void HelloTriangleApplication::createDescriptorPool()
{
    std::array poolSize = {vk::DescriptorPoolSize(vk::DescriptorType::eUniformBuffer, MAX_FRAMES_IN_FLIGHT),
                           vk::DescriptorPoolSize(vk::DescriptorType::eCombinedImageSampler, MAX_FRAMES_IN_FLIGHT)};
    vk::DescriptorPoolCreateInfo poolInfo{.flags         = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet,
                                          .maxSets       = MAX_FRAMES_IN_FLIGHT,
                                          .poolSizeCount = poolSize.size(),
                                          .pPoolSizes    = poolSize.data()};
    descriptorPool = vk::raii::DescriptorPool(device, poolInfo);
}

void HelloTriangleApplication::createDescriptorSets()
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
        vk::DescriptorImageInfo  imageInfo{.sampler     = textureSampler,
                                           .imageView   = textureImageView,
                                           .imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal};
        std::array               descriptorWrites{vk::WriteDescriptorSet{.dstSet          = descriptorSets[i],
                                                                         .dstBinding      = 0,
                                                                         .dstArrayElement = 0,
                                                                         .descriptorCount = 1,
                                                                         .descriptorType = vk::DescriptorType::eUniformBuffer,
                                                                         .pBufferInfo = &bufferInfo},
                                    vk::WriteDescriptorSet{.dstSet          = descriptorSets[i],
                                                                         .dstBinding      = 1,
                                                                         .dstArrayElement = 0,
                                                                         .descriptorCount = 1,
                                                                         .descriptorType = vk::DescriptorType::eCombinedImageSampler,
                                                                         .pImageInfo = &imageInfo}};
        device.updateDescriptorSets(descriptorWrites, {});
    }
}

void HelloTriangleApplication::copyBuffer(vk::raii::Buffer &srcBuffer, vk::raii::Buffer &dstBuffer, vk::DeviceSize size)
{
    vk::raii::CommandBuffer commandCopyBuffer = beginSimgleTimeCommands();
    commandCopyBuffer.copyBuffer(srcBuffer, dstBuffer, vk::BufferCopy(0, 0, size));
    endSingleTimeCommands(commandCopyBuffer);
}

void HelloTriangleApplication::createBuffer(vk::DeviceSize          size,
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

uint32_t HelloTriangleApplication::findMemoryType(uint32_t typeFilter, vk::MemoryPropertyFlags properties)
{
    vk::PhysicalDeviceMemoryProperties memProperties = physicalDevice.getMemoryProperties();

    for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++)
    {
        if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties)
            return i;
    }

    throw std::runtime_error("failed to find suitable memory type!");
}

void HelloTriangleApplication::createCommandBuffers()
{
    ZoneScoped;
    commandBuffers.clear();
    vk::CommandBufferAllocateInfo allocInfo{.commandPool        = commandPool,
                                            .level              = vk::CommandBufferLevel::ePrimary,
                                            .commandBufferCount = MAX_FRAMES_IN_FLIGHT};
    commandBuffers = vk::raii::CommandBuffers(device, allocInfo);
}

void HelloTriangleApplication::recordCommandBuffer(uint32_t imageIndex)
{
    auto &commandBuffer = commandBuffers[frameIndex];
    commandBuffer.begin({});

    // Before starting rendering, transition the swapchain image to
    // COLOR_ATTACHMENT_OPTIMAL
    transition_image_layout(swapChainImages[imageIndex],
                            vk::ImageLayout::eUndefined,
                            vk::ImageLayout::eColorAttachmentOptimal,
                            {},  // srcAccessMask (no need to wait for previous operations)
                            vk::AccessFlagBits2::eColorAttachmentWrite,          // dstAccessMask
                            vk::PipelineStageFlagBits2::eColorAttachmentOutput,  // srcStage
                            vk::PipelineStageFlagBits2::eColorAttachmentOutput,  // dstStage
                            vk::ImageAspectFlagBits::eColor);
    // New transition for the depth image
    transition_image_layout(
        *depthImage,
        vk::ImageLayout::eUndefined,
        vk::ImageLayout::eDepthAttachmentOptimal,
        vk::AccessFlagBits2::eDepthStencilAttachmentWrite,
        vk::AccessFlagBits2::eDepthStencilAttachmentWrite,
        vk::PipelineStageFlagBits2::eEarlyFragmentTests | vk::PipelineStageFlagBits2::eLateFragmentTests,
        vk::PipelineStageFlagBits2::eEarlyFragmentTests | vk::PipelineStageFlagBits2::eLateFragmentTests,
        vk::ImageAspectFlagBits::eDepth);
    vk::ClearValue              clearColor          = vk::ClearColorValue(0.0f, 0.0f, 0.0f, 1.0f);
    vk::ClearValue              clearDepth          = vk::ClearDepthStencilValue(1.0f, 0);
    vk::RenderingAttachmentInfo attachmentInfo      = {.imageView   = swapChainImageViews[imageIndex],
                                                       .imageLayout = vk::ImageLayout::eColorAttachmentOptimal,
                                                       .loadOp      = vk::AttachmentLoadOp::eClear,
                                                       .storeOp     = vk::AttachmentStoreOp::eStore,
                                                       .clearValue  = clearColor};
    vk::RenderingAttachmentInfo depthAttachmentInfo = {.imageView   = depthImageView,
                                                       .imageLayout = vk::ImageLayout::eDepthAttachmentOptimal,
                                                       .loadOp      = vk::AttachmentLoadOp::eClear,
                                                       .storeOp     = vk::AttachmentStoreOp::eDontCare,
                                                       .clearValue  = clearDepth};

    vk::RenderingInfo renderingInfo = {.renderArea           = {.offset = {.x = 0, .y = 0}, .extent = swapChainExtent},
                                       .layerCount           = 1,
                                       .colorAttachmentCount = 1,
                                       .pColorAttachments    = &attachmentInfo,
                                       .pDepthAttachment     = &depthAttachmentInfo};

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
    transition_image_layout(swapChainImages[imageIndex],
                            vk::ImageLayout::eColorAttachmentOptimal,
                            vk::ImageLayout::ePresentSrcKHR,
                            vk::AccessFlagBits2::eColorAttachmentWrite,
                            {},
                            vk::PipelineStageFlagBits2::eColorAttachmentOutput,
                            vk::PipelineStageFlagBits2::eBottomOfPipe,
                            vk::ImageAspectFlagBits::eColor);

    commandBuffer.end();
}

void HelloTriangleApplication::transition_image_layout(vk::Image               image,
                                                       vk::ImageLayout         oldLayout,
                                                       vk::ImageLayout         newLayout,
                                                       vk::AccessFlags2        srcAccessMask,
                                                       vk::AccessFlags2        dstAccessMask,
                                                       vk::PipelineStageFlags2 srcStageMask,
                                                       vk::PipelineStageFlags2 dstStageMask,
                                                       vk::ImageAspectFlags    image_aspect_flags)
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
                                              .image               = image,
                                              .subresourceRange    = {.aspectMask     = image_aspect_flags,
                                                                      .baseMipLevel   = 0,
                                                                      .levelCount     = 1,
                                                                      .baseArrayLayer = 0,
                                                                      .layerCount     = 1}};
    vk::DependencyInfo      dependencyInfo = {.dependencyFlags         = {},
                                              .imageMemoryBarrierCount = 1,
                                              .pImageMemoryBarriers    = &barrier};
    commandBuffers[frameIndex].pipelineBarrier2(dependencyInfo);
}

void HelloTriangleApplication::createSyncObjects()
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

void HelloTriangleApplication::updateUniformBuffer(uint32_t currentImage)
{
    static auto startTime = std::chrono::high_resolution_clock::now();

    auto  currentTime = std::chrono::high_resolution_clock::now();
    float time        = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();

    UniformBufferObject ubo{};
    ubo.model = glm::rotate(glm::mat4(1.0f), time * glm::radians(90.0f), glm::vec3(0.0f, 0.0f, 1.0f));
    ubo.view  = glm::lookAt(glm::vec3(2.0f, 2.0f, 2.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f));
    ubo.proj  = glm::perspective(glm::radians(45.0f),
                                static_cast<float>(swapChainExtent.width) / static_cast<float>(swapChainExtent.height),
                                0.1f,
                                10.0f);
    // GLM was originally designed for OpenGL, where the Y coordinate of the
    // clip coordinates is inverted. Flip the sign on the Y-axis scaling factor
    // in the projection matrix so the final image isn't rendered upside down.
    ubo.proj[1][1] *= -1;

    memcpy(uniformBuffersMapped[currentImage], &ubo, sizeof(ubo));
}

void HelloTriangleApplication::drawFrame()
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

vk::raii::ShaderModule HelloTriangleApplication::createShaderModule(const std::vector<char> &code) const
{
    ZoneScoped;
    vk::ShaderModuleCreateInfo createInfo{.codeSize = code.size() * sizeof(char),
                                          .pCode    = reinterpret_cast<const uint32_t *>(code.data())};
    vk::raii::ShaderModule     shaderModule{device, createInfo};
    return shaderModule;
}

uint32_t HelloTriangleApplication::calculateMinImageCount(const vk::SurfaceCapabilitiesKHR &surfaceCapabilities)
{
    ZoneScoped;
    auto minImageCount = std::max(3u, surfaceCapabilities.minImageCount);
    minImageCount      = (surfaceCapabilities.maxImageCount > 0 && minImageCount > surfaceCapabilities.maxImageCount)
                             ? surfaceCapabilities.maxImageCount
                             : minImageCount;
    return minImageCount;
}

vk::SurfaceFormatKHR HelloTriangleApplication::chooseSwapSurfaceFormat(
    const std::vector<vk::SurfaceFormatKHR> &availableFormats)
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

vk::PresentModeKHR HelloTriangleApplication::chooseSwapPresentMode(
    const std::vector<vk::PresentModeKHR> &availablePresentModes)
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

vk::Extent2D HelloTriangleApplication::chooseSwapExtent(const vk::SurfaceCapabilitiesKHR &capabilities)
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
        std::clamp<uint32_t>(sdl_surface->h, capabilities.minImageExtent.height, capabilities.maxImageExtent.height)};
}

std::vector<const char *> HelloTriangleApplication::getRequiredExtensions()
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

VKAPI_ATTR vk::Bool32 VKAPI_CALL
           HelloTriangleApplication::debugCallback(vk::DebugUtilsMessageSeverityFlagBitsEXT      severity,
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

bool HelloTriangleApplication::isDeviceSuitable(vk::raii::PhysicalDevice physicalDevice)
{
    ZoneScoped;
    auto deviceProperties = physicalDevice.getProperties();
    auto deviceFeatures   = physicalDevice.getFeatures();

    return (deviceProperties.deviceType == vk::PhysicalDeviceType::eDiscreteGpu && deviceFeatures.geometryShader);
}

void HelloTriangleApplication::run()
{
    initWindow();
    initVulkan();
    mainLoop();
    cleanup();
}
