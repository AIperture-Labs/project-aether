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
    explicit SDLException(const std::string& message) : std::runtime_error(message + '\n' + SDL_GetError()) {}
};

class HelloTriangleApplication {
   private:
    static constexpr uint16_t WINDOW_WIDTH = 800;
    static constexpr uint16_t WINDOW_HEIGHT = 600;
    static constexpr SDL_WindowFlags window_flags =
        SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIDDEN | SDL_WINDOW_HIGH_PIXEL_DENSITY;
    SDL_Window* window = nullptr;
    bool shouldBeClose = false;

    void initWindow() {
        if (not SDL_Init(SDL_INIT_VIDEO)) throw SDLException("SDL_Init failed");

        window = SDL_CreateWindow("Aether Game Engine", WINDOW_WIDTH, WINDOW_HEIGHT, window_flags);
        if (window == nullptr) throw SDLException("SDL_CreateWindow failed");

        SDL_ShowWindow(window);
    }

    void initVulkan() {}

    void mainLoop() {
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
    } catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}