#pragma once

#include <cstdint>
#include <stdexcept>
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
#include <tracy/Tracy.hpp>

#include "Geometry/Vextex.hpp"
#include "Utils/Handlers.hpp"

const std::vector<char const *> validationLayers = {"VK_LAYER_KHRONOS_validation"};

#if defined(_DEBUG)
constexpr bool enableValidationLayers = true;
#else
constexpr bool enableValidationLayers = false;
#endif

constexpr int MAX_FRAMES_IN_FLIGHT = 2;

const std::vector<Geometry::Vertex> vertices = {{{-0.5f, -0.5f, 0.0f}, {1.0f, 0.0f, 0.0f}, {1.0f, 0.0f}},
                                                {{0.5f, -0.5f, 0.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f}},
                                                {{0.5f, 0.5f, 0.0f}, {0.0f, 0.0f, 1.0f}, {0.0f, 1.0f}},
                                                {{-0.5f, 0.5f, 0.0f}, {1.0f, 1.0f, 1.0f}, {1.0f, 1.0f}},

                                                {{-0.5f, -0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}, {1.0f, 0.0f}},
                                                {{0.5f, -0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f}},
                                                {{0.5f, 0.5f, -0.5f}, {0.0f, 0.0f, 1.0f}, {0.0f, 1.0f}},
                                                {{-0.5f, 0.5f, -0.5f}, {1.0f, 1.0f, 1.0f}, {1.0f, 1.0f}}};

const std::vector<uint16_t> indices = {0, 1, 2, 2, 3, 0, 4, 5, 6, 6, 7, 4};

struct UniformBufferObject
{
    alignas(16) glm::mat4 model;
    alignas(16) glm::mat4 view;
    alignas(16) glm::mat4 proj;
};

/**
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
    // Members

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

    vk::raii::Image        depthImage       = nullptr;
    vk::raii::DeviceMemory depthImageMemory = nullptr;
    vk::raii::ImageView    depthImageView   = nullptr;

    vk::raii::Image        textureImage       = nullptr;
    vk::raii::DeviceMemory textureImageMemory = nullptr;
    vk::raii::ImageView    textureImageView   = nullptr;
    vk::raii::Sampler      textureSampler     = nullptr;

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

    // Methods

   private:
    void initWindow();
    void initVulkan();
    void mainLoop();
    void cleanup();
    void createInstance();
    void setupDebugMessenger();
    void createSurface();
    void pickPhysicalDevice();
    void createLogicalDevice();
    void cleanupSwapChain();
    void recreateSwapChain();
    void createSwapChain();
    void createImageViews();
    void createDescriptorSetLayout();
    void createGraphicsPipeline();
    void createCommandPool();
    void createDepthResources();

    vk::Format findSupportedFormat(const std::vector<vk::Format> &candidates,
                                   vk::ImageTiling                tiling,
                                   vk::FormatFeatureFlags         features);

    vk::Format findDepthFormat();

    bool hasStencileComponent(vk::Format format);
    void createTextureImage();
    void createImage(uint32_t                width,
                     uint32_t                height,
                     vk::Format              format,
                     vk::ImageTiling         tiling,
                     vk::ImageUsageFlags     usage,
                     vk::MemoryPropertyFlags properties,
                     vk::raii::Image        &image,
                     vk::raii::DeviceMemory &imageMemory);

    vk::raii::CommandBuffer beginSimgleTimeCommands();

    void endSingleTimeCommands(vk::raii::CommandBuffer &commandBuffer);
    void transitionImageLayout(const vk::raii::Image &image, vk::ImageLayout oldLayout, vk::ImageLayout newLayout);
    void copyBufferToImage(const vk::raii::Buffer &buffer, vk::raii::Image &image, uint32_t width, uint32_t height);
    void createTextureImageView();

    vk::raii::ImageView createImageView(vk::raii::Image &image, vk::Format format, vk::ImageAspectFlags aspectFlags);

    void     createTextureSampler();
    void     createVertexBuffer();
    void     createIndexBuffer();
    void     createUniformBuffers();
    void     createDescriptorPool();
    void     createDescriptorSets();
    void     copyBuffer(vk::raii::Buffer &srcBuffer, vk::raii::Buffer &dstBuffer, vk::DeviceSize size);
    void     createBuffer(vk::DeviceSize          size,
                          vk::BufferUsageFlags    usage,
                          vk::MemoryPropertyFlags properties,
                          vk::raii::Buffer       &buffer,
                          vk::raii::DeviceMemory &bufferMemory);
    uint32_t findMemoryType(uint32_t typeFilter, vk::MemoryPropertyFlags properties);
    void     createCommandBuffers();
    void     recordCommandBuffer(uint32_t imageIndex);
    void     transition_image_layout(vk::Image               image,
                                     vk::ImageLayout         oldLayout,
                                     vk::ImageLayout         newLayout,
                                     vk::AccessFlags2        srcAccessMask,
                                     vk::AccessFlags2        dstAccessMask,
                                     vk::PipelineStageFlags2 srcStageMask,
                                     vk::PipelineStageFlags2 dstStageMask,
                                     vk::ImageAspectFlags    image_aspect_flags);
    void     createSyncObjects();
    void     updateUniformBuffer(uint32_t currentImage);
    void     drawFrame();

    [[nodiscard]] vk::raii::ShaderModule createShaderModule(const std::vector<char> &code) const;

    static uint32_t           calculateMinImageCount(const vk::SurfaceCapabilitiesKHR &surfaceCapabilities);
    vk::SurfaceFormatKHR      chooseSwapSurfaceFormat(const std::vector<vk::SurfaceFormatKHR> &availableFormats);
    vk::PresentModeKHR        chooseSwapPresentMode(const std::vector<vk::PresentModeKHR> &availablePresentModes);
    vk::Extent2D              chooseSwapExtent(const vk::SurfaceCapabilitiesKHR &capabilities);
    std::vector<const char *> getRequiredExtensions();

    static VKAPI_ATTR vk::Bool32 VKAPI_CALL debugCallback(vk::DebugUtilsMessageSeverityFlagBitsEXT      severity,
                                                          vk::DebugUtilsMessageTypeFlagsEXT             type,
                                                          const vk::DebugUtilsMessengerCallbackDataEXT *pCallbackData,
                                                          void *);

    bool isDeviceSuitable(vk::raii::PhysicalDevice physicalDevice);

   public:
    void run();
};