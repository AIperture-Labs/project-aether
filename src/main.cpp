// Copyright (c) 2025 AIperture-Labs <xavier.beheydt@gmail.com>
// SPDX-License-Identifier: MIT
// main.cpp

#include <iostream>

// Tracy
#if defined(__clang__) || defined(__GNUC__)
#    define TracyFunction __PRETTY_FUNCTION__
#elif defined(_MSC_VER)
#    define TracyFunction __FUNCSIG__
#endif
#include <tracy/Tracy.hpp>

#include "HelloTriangleApplication.hpp"

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