#pragma once

// #if defined(_WIN32)
// #    if defined(PROJECT_BUILD_DLL)
// #        define PROJECT_API __declspec(dllexport)
// #    else
// #        define PROJECT_API __declspec(dllimport)
// #    endif
// #else
// #    define PROJECT_API
// #endif

#if defined(_WIN32)
#    define PROJECT_API __declspec(dllexport)
#else
#    define PROJECT_API
#endif