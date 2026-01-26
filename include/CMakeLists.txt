add_library(Config INTERFACE)
target_sources(Config INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/config.hpp)
target_include_directories(Config INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
add_library(Project::Config ALIAS Config)