#pragma once

#include <turbojpeg.h>

#include <string>
#include <variant>
#include <vector>

#include "config.hpp"

namespace Images {

class PROJECT_API Jpeg
{
    // Members
   public:
    static constexpr int32_t DATA_PRECISION_8_BITS  = 8;
    static constexpr int32_t DATA_PRECISION_12_BITS = 12;
    static constexpr int32_t DATA_PRECISION_16_BITS = 16;

   private:
    tjhandle                   handle;
    const std::string          filename;
    const std::vector<uint8_t> jpegBuf;
    // TODO: later make globals types
    using jpeg_sample_8_t  = uint8_t;
    using jpeg_sample_12_t = int16_t;
    using jpeg_sample_16_t = uint16_t;
    using jpeg_buf_8_t     = std::vector<jpeg_sample_8_t>;
    using jpeg_buf_12_t    = std::vector<jpeg_sample_12_t>;
    using jpeg_buf_16_t    = std::vector<jpeg_sample_16_t>;
    using jpeg_raw_buf_t   = std::variant<jpeg_buf_8_t, jpeg_buf_12_t, jpeg_buf_16_t>;
    jpeg_raw_buf_t rawBuffer;
    const TJPF     pixelFormat;

    // Methods
   public:
    Jpeg(const std::string &filename, TJPF pixelFormat = TJPF_RGBA);
    ~Jpeg(void);

    inline int32_t     getDataPrecision() const;
    inline size_t      getJpegSize() const;
    inline uint32_t    getWidth() const;
    inline uint32_t    getHeight() const;
    inline size_t      getPixelSize() const;
    inline const void *getData() const;
    inline size_t      getSize() const;

    int32_t decompress();
};

}  // namespace Images