#ifndef SIMPLE_IMAGE_H
#define SIMPLE_IMAGE_H

#define 	CV_CN_SHIFT   3
#define 	CV_DEPTH_MAX   (1 << CV_CN_SHIFT)
#define 	CV_MAT_DEPTH_MASK   (CV_DEPTH_MAX - 1)

#include <cstdint>
#include <vector>
#include <cstring>

// Simple Size structure to replace cv::Size
struct SimpleSize {
    int width;
    int height;
    
    SimpleSize() : width(0), height(0) {}
    SimpleSize(int w, int h) : width(w), height(h) {}
};

// Simple image class to replace cv::Mat
class SimpleImage {
private:
    std::vector<uint8_t> m_data;
    int m_width;
    int m_height;
    int m_channels;
    
public:
    SimpleImage() : m_width(0), m_height(0), m_channels(0) {}
    
    SimpleImage(int height, int width, int channels) 
        : m_width(width), m_height(height), m_channels(channels) {
        m_data.resize(width * height * channels);
    }
    
    SimpleImage(int height, int width, int channels, uint8_t* data) 
        : m_width(width), m_height(height), m_channels(channels) {
        size_t size = width * height * channels;
        m_data.resize(size);
        std::memcpy(m_data.data(), data, size);
    }
    
    // Copy constructor
    SimpleImage(const SimpleImage& other) 
        : m_data(other.m_data), m_width(other.m_width), 
          m_height(other.m_height), m_channels(other.m_channels) {}
    
    // Assignment operator
    SimpleImage& operator=(const SimpleImage& other) {
        if (this != &other) {
            m_data = other.m_data;
            m_width = other.m_width;
            m_height = other.m_height;
            m_channels = other.m_channels;
        }
        return *this;
    }
    
    // Clone method
    SimpleImage clone() const {
        return SimpleImage(*this);
    }
    
    // Properties
    int cols() const { return m_width; }
    int rows() const { return m_height; }
    int channels() const { return m_channels; }
    bool empty() const { return m_data.empty() || m_width == 0 || m_height == 0; }
    
    // Data access
    uint8_t* data() { return m_data.data(); }
    const uint8_t* data() const { return m_data.data(); }
    
    // Row access
    uint8_t* ptr(int row) { 
        return m_data.data() + row * m_width * m_channels; 
    }
    const uint8_t* ptr(int row) const { 
        return m_data.data() + row * m_width * m_channels; 
    }
    
    template<typename T>
    T* ptr(int row) { 
        return reinterpret_cast<T*>(m_data.data() + row * m_width * m_channels); 
    }
    
    template<typename T>
    const T* ptr(int row) const { 
        return reinterpret_cast<const T*>(m_data.data() + row * m_width * m_channels); 
    }
    
    // Create new image
    void create(int height, int width, int channels) {
        m_width = width;
        m_height = height;
        m_channels = channels;
        m_data.resize(width * height * channels);
    }
};

// Image type constants (equivalent to OpenCV)
#define SIMPLE_8UC1 1
#define SIMPLE_8UC3 3
#define SIMPLE_8UC4 4

#endif // SIMPLE_IMAGE_H