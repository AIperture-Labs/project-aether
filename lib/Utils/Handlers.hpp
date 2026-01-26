#pragma once

#include <fstream>
#include <string>
#include <vector>

#include "config.hpp"

namespace Utils::Handlers {

class PROJECT_API File
{
    // Methods
   public:
    inline static std::ifstream openFileAtEnd(const std::string &filename);

    template<typename T>
    static std::vector<T> getBuffer(const std::string &filename)
    {
        std::ifstream  file = openFileAtEnd(filename);
        std::vector<char> buffer(file.tellg() / sizeof(T));
        file.seekg(0, std::ios::beg);
        file.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
        file.close();

        return std::vector<T>(buffer.begin(), buffer.end());
    }
};

}  // namespace Utils::Handlers