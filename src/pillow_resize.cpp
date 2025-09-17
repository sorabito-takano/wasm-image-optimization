#include "pillow_resize.hpp"
#include <algorithm>
#include <cstring>
#include <stdexcept>

namespace PillowResize {

int32_t precomputeCoeffs(int32_t in_size,
                        double in0,
                        double in1,
                        int32_t out_size,
                        const LanczosFilter& filter,
                        std::vector<int32_t>& bounds,
                        std::vector<double>& kk) {
    // Prepare for horizontal stretch
    const double scale = (in1 - in0) / static_cast<double>(out_size);
    double filterscale = scale;
    if (filterscale < 1.0) {
        filterscale = 1.0;
    }

    // Determine support size (length of resampling filter)
    const double support = filter.support() * filterscale;

    // Maximum number of coeffs
    const auto k_size = static_cast<int32_t>(ceil(support)) * 2 + 1;

    // Check for overflow
    if (out_size > INT32_MAX / (k_size * static_cast<int32_t>(sizeof(double)))) {
        throw std::runtime_error("Memory error");
    }

    // Coefficient buffer
    kk.resize(out_size * k_size);

    // Bounds vector
    bounds.resize(out_size * 2);

    int32_t x = 0;
    constexpr double half_pixel = 0.5;
    
    for (int32_t xx = 0; xx < out_size; ++xx) {
        double center = in0 + (xx + half_pixel) * scale;
        double ww = 0.0;
        double ss = 1.0 / filterscale;
        
        // Round the value
        auto xmin = static_cast<int32_t>(center - support + half_pixel);
        if (xmin < 0) {
            xmin = 0;
        }
        
        // Round the value
        auto xmax = static_cast<int32_t>(center + support + half_pixel);
        if (xmax > in_size) {
            xmax = in_size;
        }
        xmax -= xmin;
        
        double* k = &kk[xx * k_size];
        
        for (x = 0; x < xmax; ++x) {
            double w = filter.filter((x + xmin - center + half_pixel) * ss);
            k[x] = w;
            ww += w;
        }
        
        for (x = 0; x < xmax; ++x) {
            if (ww != 0.0) {
                k[x] /= ww;
            }
        }
        
        // Scale coefficients for integer computation
        constexpr uint32_t precision_bits = 32 - 8 - 2;
        constexpr auto shifted_coeff = static_cast<double>(1U << precision_bits);
        constexpr double half_pixel_round = 0.5;
        
        for (x = 0; x < xmax; ++x) {
            if (k[x] < 0) {
                k[x] = trunc(-half_pixel_round + k[x] * shifted_coeff);
            } else {
                k[x] = trunc(half_pixel_round + k[x] * shifted_coeff);
            }
        }
        
        // Remaining values should stay empty if they are used despite of xmax
        for (; x < k_size; ++x) {
            k[x] = 0;
        }
        
        bounds[xx * 2 + 0] = xmin;
        bounds[xx * 2 + 1] = xmax;
    }
    
    return k_size;
}

uint8_t clip8(double in) {
    constexpr uint32_t precision_bits = 32 - 8 - 2;
    auto saturate_val = static_cast<int64_t>(in) >> precision_bits;
    if (saturate_val < 0) {
        return 0;
    }
    if (saturate_val > UINT8_MAX) {
        return UINT8_MAX;
    }
    return static_cast<uint8_t>(saturate_val);
}

template<typename T>
void resampleHorizontal(SimpleImage& im_out,
                       const SimpleImage& im_in,
                       int32_t offset,
                       int32_t ksize,
                       const std::vector<int32_t>& bounds,
                       const std::vector<double>& kk);

template<>
void resampleHorizontal<uint8_t>(SimpleImage& im_out,
                                const SimpleImage& im_in,
                                int32_t offset,
                                int32_t ksize,
                                const std::vector<int32_t>& bounds,
                                const std::vector<double>& kk) {
    int32_t ss0 = im_in.cols();
    int32_t ss1 = im_in.channels();
    
    constexpr uint32_t precision_bits = 32 - 8 - 2;
    const double init_buffer = static_cast<double>(1U << (precision_bits - 1U));
    
    for (int32_t yy = 0; yy < im_out.rows(); ++yy) {
        for (int32_t xx = 0; xx < im_out.cols(); ++xx) {
            int32_t xmin = bounds[xx * 2 + 0];
            int32_t xmax = bounds[xx * 2 + 1];
            const double* k = &kk[xx * ksize];
            
            for (int32_t c = 0; c < ss1; ++c) {
                double ss = init_buffer;
                for (int32_t x = 0; x < xmax; ++x) {
                    const uint8_t* src_ptr = im_in.ptr<uint8_t>(yy + offset) + (x + xmin) * ss1 + c;
                    ss += static_cast<double>(*src_ptr) * k[x];
                }
                
                uint8_t* dst_ptr = im_out.ptr<uint8_t>(yy) + xx * ss1 + c;
                *dst_ptr = clip8(ss);
            }
        }
    }
}

template<typename T>
void resampleVertical(SimpleImage& im_out,
                     const SimpleImage& im_in,
                     int32_t offset,
                     int32_t ksize,
                     const std::vector<int32_t>& bounds,
                     const std::vector<double>& kk);

template<>
void resampleVertical<uint8_t>(SimpleImage& im_out,
                              const SimpleImage& im_in,
                              int32_t offset,
                              int32_t ksize,
                              const std::vector<int32_t>& bounds,
                              const std::vector<double>& kk) {
    int32_t ss0 = im_in.cols();
    int32_t ss1 = im_in.channels();
    
    constexpr uint32_t precision_bits = 32 - 8 - 2;
    const double init_buffer = static_cast<double>(1U << (precision_bits - 1U));
    
    for (int32_t yy = 0; yy < im_out.rows(); ++yy) {
        int32_t ymin = bounds[yy * 2 + 0];
        int32_t ymax = bounds[yy * 2 + 1];
        const double* k = &kk[yy * ksize];
        
        for (int32_t xx = 0; xx < im_out.cols(); ++xx) {
            for (int32_t c = 0; c < ss1; ++c) {
                double ss = init_buffer;
                for (int32_t y = 0; y < ymax; ++y) {
                    const uint8_t* src_ptr = im_in.ptr<uint8_t>(y + ymin) + xx * ss1 + c;
                    ss += static_cast<double>(*src_ptr) * k[y];
                }
                
                uint8_t* dst_ptr = im_out.ptr<uint8_t>(yy) + xx * ss1 + c;
                *dst_ptr = clip8(ss);
            }
        }
    }
}

// Transpose function for SimpleImage
SimpleImage transpose(const SimpleImage& src) {
    if (src.empty()) return SimpleImage();
    
    SimpleImage dst(src.cols(), src.rows(), src.channels());
    
    for (int y = 0; y < src.rows(); y++) {
        for (int x = 0; x < src.cols(); x++) {
            for (int c = 0; c < src.channels(); c++) {
                const uint8_t* src_ptr = src.ptr<uint8_t>(y) + x * src.channels() + c;
                uint8_t* dst_ptr = dst.ptr<uint8_t>(x) + y * dst.channels() + c;
                *dst_ptr = *src_ptr;
            }
        }
    }
    
    return dst;
}

SimpleImage resize(const SimpleImage& src, const SimpleSize& out_size) {
    if (src.empty()) {
        return SimpleImage();
    }
    
    const int32_t x_size = out_size.width;
    const int32_t y_size = out_size.height;
    
    if (x_size < 1 || y_size < 1) {
        throw std::runtime_error("Output size must be positive");
    }
    
    // Create Lanczos filter
    LanczosFilter filter;
    
    SimpleImage im_out;
    SimpleImage im_temp;
    
    std::vector<int32_t> bounds_horiz;
    std::vector<int32_t> bounds_vert;
    std::vector<double> kk_horiz;
    std::vector<double> kk_vert;
    
    const bool need_horizontal = x_size != src.cols();
    const bool need_vertical = y_size != src.rows();
    
    // Compute horizontal filter coefficients
    int32_t ksize_horiz = 0;
    if (need_horizontal) {
        ksize_horiz = precomputeCoeffs(src.cols(), 0.0, 
                                      static_cast<double>(src.cols()), 
                                      x_size, filter, bounds_horiz, kk_horiz);
    }
    
    // Compute vertical filter coefficients
    int32_t ksize_vert = 0;
    if (need_vertical) {
        ksize_vert = precomputeCoeffs(src.rows(), 0.0, 
                                     static_cast<double>(src.rows()), 
                                     y_size, filter, bounds_vert, kk_vert);
    }
    
    // Two-pass resize: horizontal pass
    if (need_horizontal) {
        const int32_t ybox_first = need_vertical ? bounds_vert[0] : 0;
        const int32_t ybox_last = need_vertical ? 
            bounds_vert[y_size * 2 - 2] + bounds_vert[y_size * 2 - 1] : 
            src.rows();
        
        // Shift bounds for vertical pass
        if (need_vertical) {
            for (int32_t i = 0; i < y_size; ++i) {
                bounds_vert[i * 2] -= ybox_first;
            }
        }
        
        // Create destination image with desired output width
        im_temp.create(ybox_last - ybox_first, x_size, src.channels());
        if (!im_temp.empty()) {
            resampleHorizontal<uint8_t>(im_temp, src, ybox_first, ksize_horiz, bounds_horiz, kk_horiz);
        } else {
            throw std::runtime_error("Failed to allocate temporary image");
        }
    } else {
        im_temp = src; // No horizontal resizing needed
    }
    
    // Vertical pass
    if (need_vertical) {
        if (need_horizontal) {
            // Use horizontally resized image
            im_out.create(y_size, x_size, src.channels());
            if (!im_out.empty()) {
                resampleVertical<uint8_t>(im_out, im_temp, 0, ksize_vert, bounds_vert, kk_vert);
            } else {
                throw std::runtime_error("Failed to allocate output image");
            }
        } else {
            // Use original image for vertical-only resize
            im_out.create(y_size, x_size, src.channels());
            if (!im_out.empty()) {
                resampleVertical<uint8_t>(im_out, src, 0, ksize_vert, bounds_vert, kk_vert);
            } else {
                throw std::runtime_error("Failed to allocate output image");
            }
        }
    } else {
        im_out = im_temp; // No vertical resizing needed
    }
    
    return im_out;
}

} // namespace PillowResize
