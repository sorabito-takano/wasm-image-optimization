#include <emscripten.h>
#include <emscripten/bind.h>
#include <emscripten/val.h>
#include <webp/encode.h>
#include <webp/decode.h>

#include <webp/encode.h>
#include <webp/decode.h>

// libjpeg for JPEG decoding
#include <jpeglib.h>
#include <setjmp.h>
// libpng for PNG decoding
#include <png.h>
#include <vector>
#include <cstring>
#include <libexif/exif-data.h>

// WASM SIMD support
#ifdef __wasm__
    #ifdef __wasm_simd128__
        #include <wasm_simd128.h>
        #define HAVE_WASM_SIMD 1
    #else
        #define HAVE_WASM_SIMD 0
    #endif
#else
    #define HAVE_WASM_SIMD 0
#endif

// Include simple image processing functions
#include "simple_imgproc.h"
#include "simple_image.h"

// Include Pillow Resize for high-quality Lanczos resampling
#include "pillow_resize.hpp"

using namespace emscripten;

EM_JS(void, js_console_log, (const char *str), {
    console.log(UTF8ToString(str));
});

#if HAVE_WASM_SIMD
// SIMD-optimized BGR to RGB conversion
void convertBGRtoRGB_SIMD(const SimpleImage& src, SimpleImage& dst) {
    if (src.channels() != 3) {
        // Fallback to non-SIMD for non-RGB images
        simple_imgproc::cvtColor(src, dst, simple_imgproc::BGR2RGB);
        return;
    }
    
    dst.create(src.rows(), src.cols(), src.channels());
    
    const int width = src.cols();
    const int height = src.rows();
    const int total_pixels = width * height;
    
    const uint8_t* src_data = src.data();
    uint8_t* dst_data = dst.data();
    
    int i = 0;
    // Process 16 bytes (5+ pixels) at a time with SIMD
    for (; i <= total_pixels * 3 - 16; i += 12) {
        // Load 12 bytes (4 BGR pixels)
        v128_t bgr_pixels = wasm_v128_load(src_data + i);
        
        // Extract B, G, R channels
        // BGR BGR BGR BGR -> B0G0R0B1 G1R1B2G2 R2B3G3R3
        v128_t shuffled = wasm_i8x16_shuffle(bgr_pixels,
            wasm_i32x4_splat(0), // dummy second vector
            2, 1, 0,    // R0 G0 B0
            5, 4, 3,    // R1 G1 B1  
            8, 7, 6,    // R2 G2 B2
            11, 10, 9,  // R3 G3 B3
            14, 13, 12, // Partial next pixel
            15          // Padding
        );
        
        // Store converted RGB data
        wasm_v128_store(dst_data + i, shuffled);
    }
    
    // Process remaining pixels with scalar code
    for (; i < total_pixels * 3; i += 3) {
        uint8_t b = src_data[i];
        uint8_t g = src_data[i + 1];
        uint8_t r = src_data[i + 2];
        
        dst_data[i] = r;     // R
        dst_data[i + 1] = g; // G
        dst_data[i + 2] = b; // B
    }
}

// SIMD-optimized RGB to BGR conversion
void convertRGBtoBGR_SIMD(const SimpleImage& src, SimpleImage& dst) {
    if (src.channels() != 3) {
        // Fallback to non-SIMD for non-RGB images
        simple_imgproc::cvtColor(src, dst, simple_imgproc::RGB2BGR);
        return;
    }
    
    dst.create(src.rows(), src.cols(), src.channels());
    
    const int width = src.cols();
    const int height = src.rows();
    const int total_pixels = width * height;
    
    const uint8_t* src_data = src.data();
    uint8_t* dst_data = dst.data();
    
    int i = 0;
    // Process 12 bytes (4 RGB pixels) at a time with SIMD
    for (; i <= total_pixels * 3 - 12; i += 12) {
        // Load 12 bytes (4 RGB pixels)
        v128_t rgb_pixels = wasm_v128_load(src_data + i);
        
        // Convert RGB to BGR by shuffling
        v128_t shuffled = wasm_i8x16_shuffle(rgb_pixels,
            wasm_i32x4_splat(0), // dummy second vector
            2, 1, 0,    // B0 G0 R0
            5, 4, 3,    // B1 G1 R1  
            8, 7, 6,    // B2 G2 R2
            11, 10, 9,  // B3 G3 R3
            14, 13, 12, // Partial next pixel if available
            15          // Padding
        );
        
        // Store converted BGR data
        wasm_v128_store(dst_data + i, shuffled);
    }
    
    // Process remaining pixels with scalar code
    for (; i < total_pixels * 3; i += 3) {
        uint8_t r = src_data[i];
        uint8_t g = src_data[i + 1];
        uint8_t b = src_data[i + 2];
        
        dst_data[i] = b;     // B
        dst_data[i + 1] = g; // G
        dst_data[i + 2] = r; // R
    }
}

// SIMD-optimized memory copy for image data
void fastMemcpy_SIMD(uint8_t* dst, const uint8_t* src, size_t size) {
    size_t i = 0;
    
    // Process 16 bytes at a time with SIMD
    for (; i <= size - 16; i += 16) {
        v128_t data = wasm_v128_load(src + i);
        wasm_v128_store(dst + i, data);
    }
    
    // Process remaining bytes
    for (; i < size; ++i) {
        dst[i] = src[i];
    }
}
#endif

// Enum for image formats
enum class ImageFormat {
    JPEG,
    PNG,
    WEBP,
    UNKNOWN
};

// File format detection function
ImageFormat detectImageFormat(const uint8_t* data, size_t size) {
    if (size < 4) return ImageFormat::UNKNOWN;
    
    // JPEG starts with FF D8
    if (data[0] == 0xFF && data[1] == 0xD8) {
        return ImageFormat::JPEG;
    }
    
    // PNG: 89 50 4E 47 (PNG signature)
    if (size >= 8 && data[0] == 0x89 && data[1] == 0x50 && 
        data[2] == 0x4E && data[3] == 0x47 && data[4] == 0x0D && 
        data[5] == 0x0A && data[6] == 0x1A && data[7] == 0x0A) {
        return ImageFormat::PNG;
    }
    
    // WEBP: RIFF****WEBP パターン
    if (size >= 12 && data[0] == 'R' && data[1] == 'I' && 
        data[2] == 'F' && data[3] == 'F' &&
        data[8] == 'W' && data[9] == 'E' && 
        data[10] == 'B' && data[11] == 'P') {
        return ImageFormat::WEBP;
    }
    
    return ImageFormat::UNKNOWN;
}

class MemoryManager
{
private:
    uint8_t *m_ptr;

public:
    MemoryManager()
    {
        m_ptr = nullptr;
    }

    uint8_t *allocate(const uint8_t *data, size_t size)
    {
        uint8_t *ptr = new uint8_t[size];
#if HAVE_WASM_SIMD
        fastMemcpy_SIMD(ptr, data, size);
#else
        std::memcpy(ptr, data, size);
#endif
        m_ptr = ptr;
        return ptr;
    }

    void release()
    {
        if (m_ptr)
        {
            delete[] m_ptr;
            m_ptr = nullptr;
        }
    }
};

MemoryManager memoryManager;

val createResult(size_t size, const uint8_t *data, float originalWidth, float originalHeight, float width, float height)
{
    uint8_t *ptr = memoryManager.allocate(data, size);
    val result = val::object();
    result.set("data", val(typed_memory_view(size, ptr)));
    result.set("originalWidth", originalWidth);
    result.set("originalHeight", originalHeight);
    result.set("width", width);
    result.set("height", height);
    return result;
}

void releaseResult()
{
    memoryManager.release();
}

class ImageProcessor
{
private:
    SimpleImage m_image;
    float m_originalWidth;
    float m_originalHeight;
    int m_orientation;
    ImageFormat m_inputFormat;

    // JPEG デコード (既存の実装)
    SimpleImage decodeJPEG(const uint8_t* data, size_t size) {
        // JPEGデコード構造体の初期化
        struct jpeg_decompress_struct cinfo;
        struct jpeg_error_mgr jerr;

        cinfo.err = jpeg_std_error(&jerr);
        jpeg_create_decompress(&cinfo);

        // メモリからJPEGを読み込み
        jpeg_mem_src(&cinfo, data, size);

        // JPEGヘッダーを読み込み
        jpeg_read_header(&cinfo, TRUE);

        // デコード開始
        jpeg_start_decompress(&cinfo);

        int width = cinfo.output_width;
        int height = cinfo.output_height;
        int channels = cinfo.output_components;

        // SimpleImageを作成（RGBで受け取る）
        SimpleImage rgb_image(height, width, SIMPLE_8UC3);

        // 行ごとに読み込み
        while (cinfo.output_scanline < cinfo.output_height) {
            unsigned char* row_pointer = rgb_image.ptr<unsigned char>(cinfo.output_scanline);
            jpeg_read_scanlines(&cinfo, &row_pointer, 1);
        }

        // デコード終了とクリーンアップ
        jpeg_finish_decompress(&cinfo);
        jpeg_destroy_decompress(&cinfo);

        // RGB から BGR への変換
        SimpleImage bgr_image;
#if HAVE_WASM_SIMD
        convertRGBtoBGR_SIMD(rgb_image, bgr_image);
#else
        simple_imgproc::cvtColor(rgb_image, bgr_image, simple_imgproc::RGB2BGR);
#endif

        return bgr_image;
    }

    // WEBP デコード
    SimpleImage decodeWEBP(const uint8_t* data, size_t size) {
        int width, height;
        // RGBで直接デコード（アルファチャンネルを避ける）
        uint8_t* decoded = WebPDecodeRGB(data, size, &width, &height);
        
        if (!decoded) {
            return SimpleImage();
        }

        // RGB から BGR への変換
        SimpleImage rgb_image(height, width, SIMPLE_8UC3, decoded);
        SimpleImage bgr_image;
#if HAVE_WASM_SIMD
        convertRGBtoBGR_SIMD(rgb_image, bgr_image);
#else
        simple_imgproc::cvtColor(rgb_image, bgr_image, simple_imgproc::RGB2BGR);
#endif
        
        WebPFree(decoded);
        return bgr_image;
    }

    // PNG デコード (libpng使用)
    SimpleImage decodePNG(const uint8_t* data, size_t size) {
        // PNG読み込み用の構造体を初期化
        png_structp png = png_create_read_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);
        if (!png) {
            js_console_log("Failed to create PNG read struct");
            return SimpleImage();
        }

        png_infop info = png_create_info_struct(png);
        if (!info) {
            png_destroy_read_struct(&png, nullptr, nullptr);
            js_console_log("Failed to create PNG info struct");
            return SimpleImage();
        }

        // エラーハンドリング
        if (setjmp(png_jmpbuf(png))) {
            png_destroy_read_struct(&png, &info, nullptr);
            js_console_log("PNG decoding error");
            return SimpleImage();
        }

        // メモリからの読み込み設定
        struct png_memory_read_state {
            const uint8_t* data;
            size_t size;
            size_t pos;
        };
        
        png_memory_read_state read_state = {data, size, 0};
        
        png_set_read_fn(png, &read_state, [](png_structp png_ptr, png_bytep outBytes, png_size_t byteCountToRead) {
            png_memory_read_state* state = static_cast<png_memory_read_state*>(png_get_io_ptr(png_ptr));
            if (state->pos + byteCountToRead <= state->size) {
                memcpy(outBytes, state->data + state->pos, byteCountToRead);
                state->pos += byteCountToRead;
            } else {
                png_error(png_ptr, "Read error");
            }
        });

        // PNG情報を読み込み
        png_read_info(png, info);

        int width = png_get_image_width(png, info);
        int height = png_get_image_height(png, info);
        png_byte color_type = png_get_color_type(png, info);
        png_byte bit_depth = png_get_bit_depth(png, info);

        // 8ビットに正規化
        if (bit_depth == 16) {
            png_set_strip_16(png);
        }
        
        // パレットをRGBに変換
        if (color_type == PNG_COLOR_TYPE_PALETTE) {
            png_set_palette_to_rgb(png);
        }
        
        // グレースケールを8ビットに
        if (color_type == PNG_COLOR_TYPE_GRAY && bit_depth < 8) {
            png_set_expand_gray_1_2_4_to_8(png);
        }
        
        // 透明色をアルファチャンネルに
        if (png_get_valid(png, info, PNG_INFO_tRNS)) {
            png_set_tRNS_to_alpha(png);
        }

        png_read_update_info(png, info);

        // SimpleImageを作成
        int channels = png_get_channels(png, info);
        SimpleImage image;
        
        if (channels == 3) {
            image = SimpleImage(height, width, SIMPLE_8UC3);
        } else if (channels == 4) {
            image = SimpleImage(height, width, SIMPLE_8UC4);
        } else if (channels == 1) {
            image = SimpleImage(height, width, SIMPLE_8UC1);
        } else {
            png_destroy_read_struct(&png, &info, nullptr);
            js_console_log("Unsupported PNG channel count");
            return SimpleImage();
        }

        // 行ごとに読み込み
        std::vector<png_bytep> row_pointers(height);
        for (int y = 0; y < height; y++) {
            row_pointers[y] = image.ptr<png_byte>(y);
        }

        png_read_image(png, row_pointers.data());
        png_read_end(png, nullptr);
        png_destroy_read_struct(&png, &info, nullptr);

        // RGBA を BGR に変換
        if (channels == 4) {
            SimpleImage result;
#if HAVE_WASM_SIMD
            // RGBA to BGR は複雑なので、通常の関数を使用
            simple_imgproc::cvtColor(image, result, simple_imgproc::RGBA2BGR);
#else
            simple_imgproc::cvtColor(image, result, simple_imgproc::RGBA2BGR);
#endif
            return result;
        } else if (channels == 3) {
            SimpleImage result;
#if HAVE_WASM_SIMD
            convertRGBtoBGR_SIMD(image, result);
#else
            simple_imgproc::cvtColor(image, result, simple_imgproc::RGB2BGR);
#endif
            return result;
        } else {
            // グレースケールをBGRに変換
            SimpleImage result;
            simple_imgproc::cvtColor(image, result, simple_imgproc::GRAY2BGR);
            return result;
        }
    }

public:
    ImageProcessor(const std::string &imageData)
    {
        const uint8_t* data = reinterpret_cast<const uint8_t*>(imageData.c_str());
        size_t data_size = imageData.size();
        
        // ファイル形式を検出
        m_inputFormat = detectImageFormat(data, data_size);
        
        // 形式に応じてデコード
        switch (m_inputFormat) {
            case ImageFormat::JPEG:
                // 画像の向きを取得 (JPEG のみ EXIF サポート)
                m_orientation = getOrientation(imageData.c_str(), imageData.size());
                m_image = decodeJPEG(data, data_size);
                break;
                
            case ImageFormat::WEBP:
                m_orientation = 1; // WEBP は向き情報なし、デフォルト
                m_image = decodeWEBP(data, data_size);
                break;
                
            case ImageFormat::PNG:
                m_orientation = 1; // PNG は向き情報なし、デフォルト  
                m_image = decodePNG(data, data_size);
                break;
                
            default:
                js_console_log("Unsupported image format");
                return;
        }

        if (m_image.empty()) {
            js_console_log("Failed to decode image");
            return;
        }

        m_originalWidth = static_cast<float>(m_image.cols());
        m_originalHeight = static_cast<float>(m_image.rows());
    }

    int getOrientation(const char *data, size_t size)
    {
        int orientation = 1;
        ExifData *ed = exif_data_new_from_data((const unsigned char *)data, size);
        if (!ed)
        {
            return orientation;
        }
        ExifEntry *entry = exif_content_get_entry(ed->ifd[EXIF_IFD_0], EXIF_TAG_ORIENTATION);
        if (entry)
        {
            orientation = exif_get_short(entry->data, exif_data_get_byte_order(entry->parent->parent));
        }
        exif_data_unref(ed);
        return orientation;
    }

    bool isValid() const
    {
        return !m_image.empty();
    }

    SimpleImage resize(float width, float height)
    {
        if (m_image.empty())
        {
            return SimpleImage();
        }

        int originalWidth = m_image.cols();
        int originalHeight = m_image.rows();
        int outWidth = static_cast<int>(width);
        int outHeight = static_cast<int>(height);

        // Maintain aspect ratio when only one dimension is specified or both are specified
        float aspectSrc = static_cast<float>(originalWidth) / originalHeight;
        
        if (width > 0 && height > 0)
        {
            // Both dimensions specified - fit within bounds maintaining aspect ratio
            float aspectDest = width / height;

            if (aspectSrc > aspectDest)
            {
                outHeight = static_cast<int>(width / aspectSrc);
            }
            else
            {
                outWidth = static_cast<int>(height * aspectSrc);
            }
            
            // Don't upscale if original image is smaller than target dimensions
            if (originalWidth <= outWidth && originalHeight <= outHeight)
            {
                return applyOrientation(m_image.clone());
            }
        }
        else if (width > 0 && height <= 0)
        {
            // Only width specified - calculate height to maintain aspect ratio
            outHeight = static_cast<int>(width / aspectSrc);
            
            // Don't upscale if original width is smaller than target width
            if (originalWidth <= width)
            {
                return applyOrientation(m_image.clone());
            }
        }
        else if (height > 0 && width <= 0)
        {
            // Only height specified - calculate width to maintain aspect ratio
            outWidth = static_cast<int>(height * aspectSrc);
            
            // Don't upscale if original height is smaller than target height
            if (originalHeight <= height)
            {
                return applyOrientation(m_image.clone());
            }
        }
        else
        {
            // Neither specified - use original dimensions
            outWidth = originalWidth;
            outHeight = originalHeight;
        }

        SimpleImage resizedImage;
        
        // Use high-quality Lanczos resampling from pillow-resize
        resizedImage = PillowResize::resize(m_image, SimpleSize(outWidth, outHeight));
        
        if (resizedImage.empty()) {
            js_console_log("Pillow resize failed");
            return SimpleImage();
        }

        return applyOrientation(resizedImage);
    }

private:
    SimpleImage applyOrientation(SimpleImage image)
    {
        // rotate image if needed
        switch (m_orientation)
        {
        case 1:
            // No rotation
            break;
        case 3:
            // 180 degrees
            {
                SimpleImage rotated;
                simple_imgproc::rotate(image, rotated, simple_imgproc::ROTATE_180);
#if HAVE_WASM_SIMD
                // Use SIMD-optimized copy if available
                image.create(rotated.rows(), rotated.cols(), rotated.channels());
                fastMemcpy_SIMD(image.data(), rotated.data(), 
                               rotated.rows() * rotated.cols() * rotated.channels());
#else
                image = rotated;
#endif
            }
            break;
        case 6:
            // 90 degrees clockwise
            {
                SimpleImage rotated;
                simple_imgproc::rotate(image, rotated, simple_imgproc::ROTATE_90_CLOCKWISE);
#if HAVE_WASM_SIMD
                // Use SIMD-optimized copy if available
                image.create(rotated.rows(), rotated.cols(), rotated.channels());
                fastMemcpy_SIMD(image.data(), rotated.data(), 
                               rotated.rows() * rotated.cols() * rotated.channels());
#else
                image = rotated;
#endif
            }
            break;
        case 8:
            // 90 degrees counter-clockwise
            {
                SimpleImage rotated;
                simple_imgproc::rotate(image, rotated, simple_imgproc::ROTATE_90_COUNTERCLOCKWISE);
#if HAVE_WASM_SIMD
                // Use SIMD-optimized copy if available
                image.create(rotated.rows(), rotated.cols(), rotated.channels());
                fastMemcpy_SIMD(image.data(), rotated.data(), 
                               rotated.rows() * rotated.cols() * rotated.channels());
#else
                image = rotated;
#endif
            }
            break;
        }

        return image;
    }

public:
    float getOriginalWidth() const { return m_originalWidth; }
    float getOriginalHeight() const { return m_originalHeight; }
    SimpleImage getImage() const { return m_image; }
    ImageFormat getInputFormat() const { return m_inputFormat; }
};

// 前方宣言
std::vector<uint8_t> encodeJPEG(const SimpleImage& image, int quality);
std::vector<uint8_t> encodeWEBP(const SimpleImage& image, float quality, bool lossless);

val optimize(std::string imgData, float width, float height, float quality, std::string format)
{
    // サポートする出力形式を拡張: webp, jpeg, none
    if (format != "webp" && format != "jpeg" && format != "none")
    {
        js_console_log("Supported formats: webp, jpeg, none");
        return val::null();
    }

    ImageProcessor processor(imgData);

    if (!processor.isValid())
    {
        js_console_log("Failed to load image");
        return val::null();
    }

    // "none" format: 元画像をそのまま返す（サイズ変更なし）
    if (format == "none")
    {
        SimpleImage originalImage = processor.getImage();
        return createResult(imgData.size(),
                            reinterpret_cast<const uint8_t *>(imgData.c_str()),
                            processor.getOriginalWidth(),
                            processor.getOriginalHeight(),
                            static_cast<float>(originalImage.cols()),
                            static_cast<float>(originalImage.rows()));
    }

    // Resize image using Lanczos algorithm
    SimpleImage processedImage = processor.resize(width, height);

    if (processedImage.empty())
    {
        js_console_log("Failed to resize image");
        return val::null();
    }

    // 入力形式に応じて圧縮設定を決定
    ImageFormat inputFormat = processor.getInputFormat();
    bool shouldUseLossless = (inputFormat == ImageFormat::PNG || inputFormat == ImageFormat::WEBP);
    
    std::vector<uint8_t> encodedData;
    
    if (format == "webp") {
        // WEBP出力：入力形式に応じて可逆/非可逆を選択
        encodedData = encodeWEBP(processedImage, quality, shouldUseLossless);
        
        if (shouldUseLossless) {
            js_console_log("Using lossless WebP compression for PNG/WebP input");
        }
    } else if (format == "jpeg") {
        // JPEG出力：常に非可逆圧縮
        encodedData = encodeJPEG(processedImage, static_cast<int>(quality));
        js_console_log("Using JPEG compression");
    }
    
    if (encodedData.empty()) {
        js_console_log("Failed to encode image");
        return val::null();
    }

    val result = createResult(encodedData.size(), encodedData.data(),
                              processor.getOriginalWidth(),
                              processor.getOriginalHeight(),
                              static_cast<float>(processedImage.cols()),
                              static_cast<float>(processedImage.rows()));

    return result;
}

// JPEG エンコード関数
std::vector<uint8_t> encodeJPEG(const SimpleImage& image, int quality) {
    std::vector<uint8_t> result;
    
    // JPEG圧縮用の構造体を初期化
    struct jpeg_compress_struct cinfo;
    struct jpeg_error_mgr jerr;
    
    cinfo.err = jpeg_std_error(&jerr);
    jpeg_create_compress(&cinfo);
    
    // メモリ出力の設定
    unsigned char* outbuffer = nullptr;
    unsigned long outsize = 0;
    jpeg_mem_dest(&cinfo, &outbuffer, &outsize);
    
    // 画像サイズと形式の設定
    cinfo.image_width = image.cols();
    cinfo.image_height = image.rows();
    cinfo.input_components = 3;
    cinfo.in_color_space = JCS_RGB;
    
    jpeg_set_defaults(&cinfo);
    jpeg_set_quality(&cinfo, quality, TRUE);
    
    // 圧縮開始
    jpeg_start_compress(&cinfo, TRUE);
    
    // BGR to RGB 変換とエンコード
    SimpleImage rgb_image;
#if HAVE_WASM_SIMD
    convertBGRtoRGB_SIMD(image, rgb_image);
#else
    simple_imgproc::cvtColor(image, rgb_image, simple_imgproc::BGR2RGB);
#endif
    
    while (cinfo.next_scanline < cinfo.image_height) {
        JSAMPROW row_pointer = rgb_image.ptr<JSAMPLE>(cinfo.next_scanline);
        jpeg_write_scanlines(&cinfo, &row_pointer, 1);
    }
    
    jpeg_finish_compress(&cinfo);
    
    // 結果をvectorにコピー
    result.assign(outbuffer, outbuffer + outsize);
    
    // クリーンアップ
    if (outbuffer) {
        free(outbuffer);
    }
    jpeg_destroy_compress(&cinfo);
    
    return result;
}

// WEBP エンコード関数（可逆・非可逆対応）
std::vector<uint8_t> encodeWEBP(const SimpleImage& image, float quality, bool lossless) {
    std::vector<uint8_t> result;
    
    // BGR to RGB 変換
    SimpleImage rgb_image;
#if HAVE_WASM_SIMD
    convertBGRtoRGB_SIMD(image, rgb_image);
#else
    simple_imgproc::cvtColor(image, rgb_image, simple_imgproc::BGR2RGB);
#endif
    
    uint8_t* webpData = nullptr;
    size_t webpSize = 0;
    
    if (lossless) {
        // 可逆圧縮
        webpSize = WebPEncodeLosslessRGB(rgb_image.data(), rgb_image.cols(), rgb_image.rows(), 
                                       rgb_image.cols() * 3, &webpData);
    } else {
        // 非可逆圧縮（既存の実装）
        webpSize = WebPEncodeRGB(rgb_image.data(), rgb_image.cols(), rgb_image.rows(), 
                                rgb_image.cols() * 3, quality, &webpData);
    }
    
    if (webpSize > 0 && webpData) {
        result.assign(webpData, webpData + webpSize);
        WebPFree(webpData);
    }
    
    return result;
}

EMSCRIPTEN_BINDINGS(my_module)
{
    function("optimize", &optimize);
    function("releaseResult", &releaseResult);
}
