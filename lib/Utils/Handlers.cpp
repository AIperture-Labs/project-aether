#include "Handlers.hpp"

namespace Utils::Handlers {

inline std::ifstream File::openFileAtEnd(const std::string &filename)
{
    std::ifstream file(filename, std::ios::ate | std::ios::binary);
    if (!file.is_open())
    {
        throw std::runtime_error("failed to open file!");
    }
    return file;
}

// template<typename T>
// std::vector<T> File::getBuffer(const std::string &filename)
// {
//     std::ifstream     file = openFileAtEnd(filename);
//     std::vector<T> buffer(file.tellg() / sizeof(T));
//     file.seekg(0, std::ios::beg);
//     file.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
//     file.close();

//     return std::vector<T>(buffer.begin(), buffer.end());
// }

}  // namespace Utils::Handlers