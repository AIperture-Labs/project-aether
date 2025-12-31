# Copyright (c) 2025 AIperture-Labs <xavier.beheydt@gmail.com>
# SPDX-License-Identifier: MIT

add_executable(helloworld helloworld.cpp)

# Register the HelloWorld executable as a CTest test
if (ENABLE_TESTING)
    add_test(NAME HelloWorldTest COMMAND helloworld)
endif()