#ifndef SIMPLE_IMGPROC_H
#define SIMPLE_IMGPROC_H

#include "simple_image.h"
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
void cvtColor(const SimpleImage& src, SimpleImage& dst, ColorConversion conversion);

// Simple rotation function
void rotate(const SimpleImage& src, SimpleImage& dst, RotationType rotation);

} // namespace simple_imgproc

#endif // SIMPLE_IMGPROC_H