#ifndef PILLOW_RESIZE_HPP
#define PILLOW_RESIZE_HPP

#include "simple_image.h"
#include <memory>
#include <vector>
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace PillowResize {
    // Lanczos filter implementation extracted from pillow-resize
    class LanczosFilter {
    private:
        static constexpr double lanczos_filter_support = 3.0;
        
        static double _sincFilter(double x) {
            if (x == 0.0) {
                return 1.0;
            }
            x = x * M_PI;
            return sin(x) / x;
        }
        
    public:
        double support() const { return lanczos_filter_support; }
        
        double filter(double x) const {
            // Truncated sinc filter (Lanczos kernel with a = 3)
            constexpr double lanczos_a_param = 3.0;
            if (-lanczos_a_param <= x && x < lanczos_a_param) {
                return _sincFilter(x) * _sincFilter(x / lanczos_a_param);
            }
            return 0.0;
        }
    };
    
    // Precompute coefficients for 1D interpolation
    int32_t precomputeCoeffs(int32_t in_size,
                            double in0,
                            double in1,
                            int32_t out_size,
                            const LanczosFilter& filter,
                            std::vector<int32_t>& bounds,
                            std::vector<double>& kk);
    
    // Optimized clipping function for 8-bit values
    uint8_t clip8(double in);
    
    // Horizontal resampling function
    template<typename T>
    void resampleHorizontal(SimpleImage& im_out,
                           const SimpleImage& im_in,
                           int32_t offset,
                           int32_t ksize,
                           const std::vector<int32_t>& bounds,
                           const std::vector<double>& kk);
    
    // Vertical resampling function
    void resampleVertical(SimpleImage& im_out,
                         const SimpleImage& im_in,
                         int32_t ksize,
                         const std::vector<int32_t>& bounds,
                         const std::vector<double>& kk);
    
    // Main resize function using Lanczos resampling
    SimpleImage resize(const SimpleImage& src, const SimpleSize& out_size);
}

#endif // PILLOW_RESIZE_HPP
