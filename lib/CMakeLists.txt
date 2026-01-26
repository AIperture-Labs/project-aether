add_library(Jpeg SHARED Images/Jpeg.cpp)
target_include_directories(Jpeg PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}) # expose headers to consumers
target_link_libraries(Jpeg $<IF:$<TARGET_EXISTS:libjpeg-turbo::turbojpeg>,libjpeg-turbo::turbojpeg,libjpeg-turbo::turbojpeg-static>)
target_compile_definitions(Jpeg PRIVATE JPEG_BUILD_DLL) # control __declspec(dllexport)
# Place generated DLL next to the executable build output so the loader can find it at runtime.
set_target_properties(Jpeg PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/src
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib
    ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib
    CXX_STANDARD 20
)
add_library(Images::Jpeg ALIAS Jpeg)


add_library(Vertex INTERFACE)
set_target_properties(Vertex PROPERTIES CXX_STANDARD 20)
target_include_directories(Vertex
    INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/Geometry
    ${Vulkan_INCLUDE_DIR}
)
target_sources(Vertex INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/Geometry/Vextex.hpp)
target_link_libraries(Vertex INTERFACE Vulkan::cppm glm::glm)
add_library(Geometry::Vertex ALIAS Vertex)

add_library(Utils SHARED Utils/Handlers.cpp)
target_include_directories(Utils PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(Utils Project::Config)
set_target_properties(Utils PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/src
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib
    ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib
    CXX_STANDARD 20
)
