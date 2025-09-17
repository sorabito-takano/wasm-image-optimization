#ifndef SIMPLE_IMGPROC_H
#define SIMPLE_IMGPROC_H

#include <opencv2/core.hpp>
#include <cstdint>

namespace simple_imgproc {

// Color conversion types
enum ColorConversion {
    RGB2BGR,
    BGR2RGB,
    RGBA2BGR,
    GRAY2BGR
};

// Rotation types
enum RotationType {
    ROTATE_90_CLOCKWISE,
    ROTATE_180,
    ROTATE_90_COUNTERCLOCKWISE
};

// Simple color conversion function
void cvtColor(const cv::Mat& src, cv::Mat& dst, ColorConversion conversion);

// Simple rotation function
void rotate(const cv::Mat& src, cv::Mat& dst, RotationType rotation);

} // namespace simple_imgproc

#endif // SIMPLE_IMGPROC_H