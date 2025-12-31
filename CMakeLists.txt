# Copyright (c) 2025 Xavier Beheydt <xavier.beheydt@gmail.com>

cmake_minimum_required(VERSION 3.20)

# Root Settings
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

project(
    AetherEngine
    VERSION 0.1.0
)

add_subdirectory(src)
