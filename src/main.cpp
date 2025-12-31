// main.cpp
#if defined(__INTELLISENSE__) || !defined(USE_CPP20_MODULE)
#include <vulkan/vulkan_raii.hpp>
#else
import vulkan_hpp;
#endif

// #define SDL_MAIN_USE_CALLBACKS // This is necessary for the new callbacks API. To use the legacy API, don't define
// this.
#include <SDL3/SDL.h>
// #include <SDL3/SDL_init.h>
// #include <SDL3/SDL_main.h>
// #include <SDL3/SDL_vulkan.h>

#include <cstdlib>
#include <iostream>
#include <stdexcept>

SDL_AppResult SDL_Fail() {
    SDL_LogError(SDL_LOG_CATEGORY_CUSTOM, "Error %s", SDL_GetError());
    return SDL_APP_FAILURE;
}

class HelloTriangleApplication {
  private:
    void initWindow() {
        if (not SDL_Init(SDL_INIT_VIDEO))
            throw std::runtime_error(SDL_GetError());
    }
    void initVulkan() {}
    void mainLoop() {}
    void cleanup() {}

  public:
    void run() {
        initWindow();
        initVulkan();
        mainLoop();
        cleanup();
    }
};

// Replace main with SDL3.
// SDL_AppResult SDL_AppInit(void **appstate, int argc, char *argv[]) {
int main() {
    SDL_Log("%s", "Hello SDL!");
    HelloTriangleApplication app;

    try {
        app.run();
    } catch (const std::exception &e) {
        std::cerr << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}