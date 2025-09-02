#include <emscripten.h>
#include <emscripten/bind.h>
#include <emscripten/val.h>
#include <webp/encode.h>
#include <webp/decode.h>
#include <opencv2/opencv.hpp>
#include <opencv2/imgproc.hpp>
#include <vector>
#include <cstring>

using namespace emscripten;
using namespace cv;

EM_JS(void, js_console_log, (const char *str), {
    console.log(UTF8ToString(str));
});

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
        std::memcpy(ptr, data, size);
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
    Mat m_image;
    float m_originalWidth;
    float m_originalHeight;

public:
    ImageProcessor(const std::string& imageData)
    {
        // Decode image from memory using OpenCV
        std::vector<uchar> buffer(imageData.begin(), imageData.end());
        m_image = imdecode(buffer, IMREAD_COLOR);
        
        if (!m_image.empty())
        {
            m_originalWidth = static_cast<float>(m_image.cols);
            m_originalHeight = static_cast<float>(m_image.rows);
            
            // Convert BGR to RGB for WebP encoding compatibility
            cvtColor(m_image, m_image, COLOR_BGR2RGB);
        }
        else
        {
            m_originalWidth = 0;
            m_originalHeight = 0;
        }
    }

    bool isValid() const
    {
        return !m_image.empty();
    }

    Mat resize(float width, float height)
    {
        if (m_image.empty())
        {
            return Mat();
        }

        int outWidth = width > 0 ? static_cast<int>(width) : m_image.cols;
        int outHeight = height > 0 ? static_cast<int>(height) : m_image.rows;

        // Maintain aspect ratio
        if (width > 0 && height > 0)
        {
            float aspectSrc = static_cast<float>(m_image.cols) / m_image.rows;
            float aspectDest = width / height;

            if (aspectSrc > aspectDest)
            {
                outHeight = static_cast<int>(width / aspectSrc);
            }
            else
            {
                outWidth = static_cast<int>(height * aspectSrc);
            }
        }

        Mat resizedImage;
        // Use Lanczos algorithm (INTER_LANCZOS4) for high-quality resampling
        cv::resize(m_image, resizedImage, Size(outWidth, outHeight), 0, 0, INTER_LANCZOS4);
        
        return resizedImage;
    }

    float getOriginalWidth() const { return m_originalWidth; }
    float getOriginalHeight() const { return m_originalHeight; }
    Mat getImage() const { return m_image; }
};

val optimize(std::string imgData, float width, float height, float quality, std::string format)
{
    // WebP専用化: formatパラメータは後方互換性のために残すが、WebP以外は受け付けない
    if (format != "webp" && format != "none")
    {
        js_console_log("Only WebP format is supported in this version");
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
        Mat originalImage = processor.getImage();
        return createResult(imgData.size(), 
                          reinterpret_cast<const uint8_t*>(imgData.c_str()), 
                          processor.getOriginalWidth(), 
                          processor.getOriginalHeight(),
                          static_cast<float>(originalImage.cols), 
                          static_cast<float>(originalImage.rows));
    }

    // Resize image using Lanczos algorithm
    Mat processedImage = processor.resize(width, height);
    
    if (processedImage.empty())
    {
        js_console_log("Failed to resize image");
        return val::null();
    }

    // Convert to RGBA for WebP encoding
    if (processedImage.channels() == 3)
    {
        cvtColor(processedImage, processedImage, COLOR_RGB2RGBA);
    }

    // Encode to WebP
    uint8_t* webpData;
    size_t webpSize = WebPEncodeRGBA(
        processedImage.data, 
        processedImage.cols, 
        processedImage.rows, 
        processedImage.cols * 4, 
        quality, 
        &webpData
    );

    if (webpSize == 0 || !webpData)
    {
        js_console_log("Failed to encode WebP");
        return val::null();
    }

    val result = createResult(webpSize, webpData, 
                            processor.getOriginalWidth(), 
                            processor.getOriginalHeight(),
                            static_cast<float>(processedImage.cols), 
                            static_cast<float>(processedImage.rows));
    
    WebPFree(webpData);
    return result;
}

EMSCRIPTEN_BINDINGS(my_module)
{
    function("optimize", &optimize);
    function("releaseResult", &releaseResult);
}
