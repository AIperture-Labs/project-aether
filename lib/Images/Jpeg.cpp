#include "Jpeg.hpp"

#include <fstream>
#include <stdexcept>

namespace Images {

JPEG_API Jpeg::Jpeg(const std::string &filename, TJPF pixelFormat) :
    handle(tj3Init(TJINIT_DECOMPRESS)), filename(filename), jpegBuf(readJpeg(filename)), pixelFormat(pixelFormat)
{
    if (handle == nullptr)
    {
        throw std::runtime_error(std::string("Failed to initialize TurboJPEG context : ") + tj3GetErrorStr(handle));
    }
    if (tj3DecompressHeader(handle, jpegBuf.data(), getJpegSize()) != 0)
    {
        throw std::runtime_error(std::string("Failed to decompress JPEG header: ") + tj3GetErrorStr(handle));
    }
    if (decompress() != 0)
    {
        throw std::runtime_error(std::string("Failed to decompress JPEG: ") + tj3GetErrorStr(handle));
    }
}

JPEG_API Jpeg::~Jpeg(void)
{
    tj3Destroy(handle);
}

inline int32_t Jpeg::getDataPrecision() const
{
    return tj3Get(handle, TJPARAM_PRECISION);
}

inline size_t Jpeg::getJpegSize() const
{
    return jpegBuf.size();
}

inline uint32_t Jpeg::getWidth() const
{
    return tj3Get(handle, TJPARAM_JPEGWIDTH);
}

inline uint32_t Jpeg::getHeight() const
{
    return tj3Get(handle, TJPARAM_JPEGHEIGHT);
}

inline size_t Jpeg::getPixelSize() const
{
    return tjPixelSize[pixelFormat];
}

inline const void *Jpeg::getData() const
{
    return std::visit([](auto const &_buf) -> const void * { return _buf.data(); }, rawBuffer);
}

inline size_t Jpeg::getSize() const
{
    return getWidth() * getHeight() * getPixelSize();
}

JPEG_API int32_t Jpeg::decompress()
{
    if (getDataPrecision() == DATA_PRECISION_8_BITS)
    {
        auto &_buf = std::get<jpeg_buf_8_t>(rawBuffer);
        _buf.resize(getSize());
        return tj3Decompress8(handle, jpegBuf.data(), getSize(), _buf.data(), getWidth() * getPixelSize(), pixelFormat);
    }
    else if (getDataPrecision() == DATA_PRECISION_12_BITS)
    {
        auto &_buf = std::get<jpeg_buf_12_t>(rawBuffer);
        _buf.resize(getSize());
        return tj3Decompress12(handle,
                               jpegBuf.data(),
                               getSize(),
                               _buf.data(),
                               getWidth() * getPixelSize(),
                               pixelFormat);
    }
    else if (getDataPrecision() == DATA_PRECISION_16_BITS)
    {
        auto &_buf = std::get<jpeg_buf_16_t>(rawBuffer);
        _buf.resize(getSize());
        return tj3Decompress16(handle,
                               jpegBuf.data(),
                               getSize(),
                               _buf.data(),
                               getWidth() * getPixelSize(),
                               pixelFormat);
    }
    else
    {
        return -1;
    }
}

JPEG_API std::vector<uint8_t> Jpeg::readJpeg(const std::string &filename)
{
    std::ifstream file(filename, std::ios::ate | std::ios::binary);

    if (!file.is_open())
    {
        throw std::runtime_error("failed to open file!");
    }

    std::vector<char> buffer(file.tellg());
    file.seekg(0, std::ios::beg);
    file.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
    file.close();

    return std::vector<uint8_t>(buffer.begin(), buffer.end());
}

}  // namespace Images