#include "pillow_resize.hpp"
#include <algorithm>
#include <cstring>

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

template<>
void resampleHorizontal<uint8_t>(cv::Mat& im_out,
                                const cv::Mat& im_in,
                                int32_t offset,
                                int32_t ksize,
                                const std::vector<int32_t>& bounds,
                                const std::vector<double>& prekk) {
    // Normalize coefficients for 8-bit processing
    std::vector<double> kk;
    kk.reserve(prekk.size());
    
    constexpr uint32_t precision_bits = 32 - 8 - 2;
    constexpr auto shifted_coeff = static_cast<double>(1U << precision_bits);
    constexpr double half_pixel = 0.5;
    
    for (const auto& k : prekk) {
        if (k < 0) {
            kk.emplace_back(trunc(-half_pixel + k * shifted_coeff));
        } else {
            kk.emplace_back(trunc(half_pixel + k * shifted_coeff));
        }
    }
    
    const double init_buffer = static_cast<double>(1U << (precision_bits - 1U));

    for (int32_t yy = 0; yy < im_out.size().height; ++yy) {
        for (int32_t xx = 0; xx < im_out.size().width; ++xx) {
            const int32_t xmin = bounds[xx * 2 + 0];
            const int32_t xmax = bounds[xx * 2 + 1];
            const double* k = &kk[xx * ksize];

            for (int32_t c = 0; c < im_in.channels(); ++c) {
                double ss = init_buffer;
                for (int32_t x = 0; x < xmax; ++x) {
                    ss += static_cast<double>(im_in.ptr<uint8_t>(yy + offset, x + xmin)[c]) * k[x];
                }
                im_out.ptr<uint8_t>(yy, xx)[c] = clip8(ss);
            }
        }
    }
}

template<>
void resampleHorizontal<float>(cv::Mat& im_out,
                              const cv::Mat& im_in,
                              int32_t offset,
                              int32_t ksize,
                              const std::vector<int32_t>& bounds,
                              const std::vector<double>& kk) {
    for (int32_t yy = 0; yy < im_out.size().height; ++yy) {
        for (int32_t xx = 0; xx < im_out.size().width; ++xx) {
            const int32_t xmin = bounds[xx * 2 + 0];
            const int32_t xmax = bounds[xx * 2 + 1];
            const double* k = &kk[xx * ksize];

            for (int32_t c = 0; c < im_in.channels(); ++c) {
                double ss = 0.0;
                for (int32_t x = 0; x < xmax; ++x) {
                    ss += static_cast<double>(im_in.ptr<float>(yy + offset, x + xmin)[c]) * k[x];
                }
                im_out.ptr<float>(yy, xx)[c] = static_cast<float>(ss);
            }
        }
    }
}

void resampleVertical(cv::Mat& im_out,
                     const cv::Mat& im_in,
                     int32_t ksize,
                     const std::vector<int32_t>& bounds,
                     const std::vector<double>& kk) {
    // Transpose matrices for vertical resampling
    cv::Mat im_out_t = im_out.t();
    cv::Mat im_in_t = im_in.t();
    
    int pixel_type = im_in.type() & CV_MAT_DEPTH_MASK;
    
    if (pixel_type == CV_8U) {
        resampleHorizontal<uint8_t>(im_out_t, im_in_t, 0, ksize, bounds, kk);
    } else if (pixel_type == CV_32F) {
        resampleHorizontal<float>(im_out_t, im_in_t, 0, ksize, bounds, kk);
    } else {
        throw std::runtime_error("Unsupported pixel type for Lanczos resampling");
    }
    
    im_out = im_out_t.t();
}

cv::Mat resize(const cv::Mat& src, const cv::Size& out_size) {
    if (src.empty()) {
        return cv::Mat();
    }
    
    const int32_t x_size = out_size.width;
    const int32_t y_size = out_size.height;
    
    if (x_size < 1 || y_size < 1) {
        throw std::runtime_error("Output size must be positive");
    }
    
    // Create Lanczos filter
    LanczosFilter filter;
    
    cv::Mat im_out;
    cv::Mat im_temp;
    
    std::vector<int32_t> bounds_horiz;
    std::vector<int32_t> bounds_vert;
    std::vector<double> kk_horiz;
    std::vector<double> kk_vert;
    
    const bool need_horizontal = x_size != src.size().width;
    const bool need_vertical = y_size != src.size().height;
    
    // Compute horizontal filter coefficients
    int32_t ksize_horiz = 0;
    if (need_horizontal) {
        ksize_horiz = precomputeCoeffs(src.size().width, 0.0, 
                                      static_cast<double>(src.size().width), 
                                      x_size, filter, bounds_horiz, kk_horiz);
    }
    
    // Compute vertical filter coefficients
    int32_t ksize_vert = 0;
    if (need_vertical) {
        ksize_vert = precomputeCoeffs(src.size().height, 0.0, 
                                     static_cast<double>(src.size().height), 
                                     y_size, filter, bounds_vert, kk_vert);
    }
    
    // Two-pass resize: horizontal pass
    if (need_horizontal) {
        const int32_t ybox_first = need_vertical ? bounds_vert[0] : 0;
        const int32_t ybox_last = need_vertical ? 
            bounds_vert[y_size * 2 - 2] + bounds_vert[y_size * 2 - 1] : 
            src.size().height;
        
        // Shift bounds for vertical pass
        if (need_vertical) {
            for (int32_t i = 0; i < y_size; ++i) {
                bounds_vert[i * 2] -= ybox_first;
            }
        }
        
        // Create destination image with desired output width
        im_temp.create(ybox_last - ybox_first, x_size, src.type());
        if (!im_temp.empty()) {
            int pixel_type = src.type() & CV_MAT_DEPTH_MASK;
            
            if (pixel_type == CV_8U) {
                resampleHorizontal<uint8_t>(im_temp, src, ybox_first, ksize_horiz, bounds_horiz, kk_horiz);
            } else if (pixel_type == CV_32F) {
                resampleHorizontal<float>(im_temp, src, ybox_first, ksize_horiz, bounds_horiz, kk_horiz);
            } else {
                throw std::runtime_error("Unsupported pixel type for Lanczos resampling");
            }
        } else {
            return cv::Mat();
        }
        im_out = im_temp;
    }
    
    // Vertical pass
    if (need_vertical) {
        const auto new_w = (im_temp.size().width != 0) ? im_temp.size().width : x_size;
        im_out.create(y_size, new_w, src.type());
        if (!im_out.empty()) {
            if (im_temp.empty()) {
                im_temp = src;
            }
            // Input can be the original image or horizontally resampled one
            resampleVertical(im_out, im_temp, ksize_vert, bounds_vert, kk_vert);
        } else {
            return cv::Mat();
        }
    }
    
    // None of the previous steps are performed, copying
    if (im_out.empty()) {
        im_out = src;
    }
    
    return im_out;
}

// Explicit template instantiations
template void resampleHorizontal<uint8_t>(cv::Mat& im_out,
                                         const cv::Mat& im_in,
                                         int32_t offset,
                                         int32_t ksize,
                                         const std::vector<int32_t>& bounds,
                                         const std::vector<double>& kk);

template void resampleHorizontal<float>(cv::Mat& im_out,
                                       const cv::Mat& im_in,
                                       int32_t offset,
                                       int32_t ksize,
                                       const std::vector<int32_t>& bounds,
                                       const std::vector<double>& kk);

} // namespace PillowResize
