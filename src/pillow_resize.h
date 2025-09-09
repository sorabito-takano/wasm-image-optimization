#ifndef PILLOW_RESIZE_H
#define PILLOW_RESIZE_H

#include <cstdint>
#include <vector>
#include <cmath>

namespace pillow_resize {

// Lanczos filter implementation
class LanczosFilter {
public:
    static const int RADIUS = 3;  // Lanczos3
    
    static double filter(double x) {
        if (x == 0.0) return 1.0;
        if (x < 0.0) x = -x;
        if (x >= RADIUS) return 0.0;
        
        x *= M_PI;
        return RADIUS * sin(x) * sin(x / RADIUS) / (x * x);
    }
};

// Resize using Lanczos resampling
void resize_lanczos(
    const uint8_t* src_data, int src_width, int src_height, int src_channels,
    uint8_t* dst_data, int dst_width, int dst_height
);

} // namespace pillow_resize

#endif // PILLOW_RESIZE_H
