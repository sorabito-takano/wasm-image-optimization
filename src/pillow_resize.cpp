#include "pillow_resize.hpp"
#include <algorithm>
#include <cstring>
#include <stdexcept>

#if HAVE_WASM_SIMD
// SIMD optimized clipping function
v128_t clip8_v128(v128_t in) {
    constexpr uint32_t precision_bits = 32 - 8 - 2;
    
    // Shift right to get proper values
    v128_t shifted = wasm_i32x4_shr(in, precision_bits);
    
    // Clamp to 0-255 range
    v128_t zero = wasm_i32x4_splat(0);
    v128_t max_val = wasm_i32x4_splat(255);
    v128_t clamped = wasm_i32x4_max(zero, wasm_i32x4_min(shifted, max_val));
    
    // Convert to 8-bit values  
    return wasm_i16x8_narrow_i32x4(clamped, clamped);
}
#endif

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
        
        // Scale coefficients for integer computation with SIMD optimization
        constexpr uint32_t precision_bits = 32 - 8 - 2;
        constexpr auto shifted_coeff = static_cast<double>(1U << precision_bits);
        constexpr double half_pixel_round = 0.5;
        
        // Process coefficients with SIMD when possible
        int simd_x = 0;
        for (; simd_x <= xmax - 4; simd_x += 4) {
            v128_t coeff_vec = wasm_f64x2_make(k[simd_x], k[simd_x + 1]);
            v128_t coeff_vec2 = wasm_f64x2_make(k[simd_x + 2], k[simd_x + 3]);
            v128_t shifted_vec = wasm_f64x2_splat(shifted_coeff);
            v128_t half_round_pos = wasm_f64x2_splat(half_pixel_round);
            v128_t half_round_neg = wasm_f64x2_splat(-half_pixel_round);
            v128_t zero_vec = wasm_f64x2_splat(0.0);
            
            // Process first pair
            v128_t scaled1 = wasm_f64x2_mul(coeff_vec, shifted_vec);
            v128_t mask1 = wasm_f64x2_lt(coeff_vec, zero_vec);
            v128_t offset1 = wasm_v128_bitselect(half_round_neg, half_round_pos, mask1);
            v128_t result1 = wasm_f64x2_add(scaled1, offset1);
            
            // Process second pair
            v128_t scaled2 = wasm_f64x2_mul(coeff_vec2, shifted_vec);
            v128_t mask2 = wasm_f64x2_lt(coeff_vec2, zero_vec);
            v128_t offset2 = wasm_v128_bitselect(half_round_neg, half_round_pos, mask2);
            v128_t result2 = wasm_f64x2_add(scaled2, offset2);
            
            // Extract results and truncate
            k[simd_x] = trunc(wasm_f64x2_extract_lane(result1, 0));
            k[simd_x + 1] = trunc(wasm_f64x2_extract_lane(result1, 1));
            k[simd_x + 2] = trunc(wasm_f64x2_extract_lane(result2, 0));
            k[simd_x + 3] = trunc(wasm_f64x2_extract_lane(result2, 1));
        }
        
        // Process remaining coefficients with scalar code
        for (x = simd_x; x < xmax; ++x) {
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

// SIMD-optimized clip function for multiple values
void clip8_simd(const double* input, uint8_t* output, int count) {
    constexpr uint32_t precision_bits = 32 - 8 - 2;
    const v128_t zero = wasm_i32x4_splat(0);
    const v128_t max_val = wasm_i32x4_splat(255);
    
    int i = 0;
    // Process 4 values at a time with SIMD
    for (; i <= count - 4; i += 4) {
        // Convert doubles to integers with right shift
        v128_t vals = wasm_i32x4_make(
            static_cast<int32_t>(static_cast<int64_t>(input[i]) >> precision_bits),
            static_cast<int32_t>(static_cast<int64_t>(input[i+1]) >> precision_bits),
            static_cast<int32_t>(static_cast<int64_t>(input[i+2]) >> precision_bits),
            static_cast<int32_t>(static_cast<int64_t>(input[i+3]) >> precision_bits)
        );
        
        // Clamp to [0, 255] range
        v128_t clamped = wasm_i32x4_max(zero, wasm_i32x4_min(vals, max_val));
        
        // Extract and store results
        output[i] = static_cast<uint8_t>(wasm_i32x4_extract_lane(clamped, 0));
        output[i+1] = static_cast<uint8_t>(wasm_i32x4_extract_lane(clamped, 1));
        output[i+2] = static_cast<uint8_t>(wasm_i32x4_extract_lane(clamped, 2));
        output[i+3] = static_cast<uint8_t>(wasm_i32x4_extract_lane(clamped, 3));
    }
    
    // Process remaining values with scalar code
    for (; i < count; ++i) {
        output[i] = clip8(input[i]);
    }
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

#if HAVE_WASM_SIMD
// SIMD-optimized horizontal resampling for uint8_t
void resampleHorizontalSIMD(SimpleImage& im_out,
                            const SimpleImage& im_in,
                            int32_t offset,
                            int32_t ksize,
                            const std::vector<int32_t>& bounds,
                            const std::vector<double>& kk) {
    const int32_t ss0 = im_in.cols();
    const int32_t ss1 = im_in.channels();
    
    constexpr uint32_t precision_bits = 32 - 8 - 2;
    const double init_buffer = static_cast<double>(1U << (precision_bits - 1U));
    
    // Process pixels with both SIMD and scalar fallback
    for (int32_t yy = 0; yy < im_out.rows(); ++yy) {
        for (int32_t xx = 0; xx < im_out.cols(); ++xx) {
            int32_t xmin = bounds[xx * 2 + 0];
            int32_t xmax = bounds[xx * 2 + 1];
            const double* k = &kk[xx * ksize];
            
            // For RGB/RGBA channels, process with optimized code
            if (ss1 == 3 || ss1 == 4) {
                // Accumulate results for all channels
                double results[4] = {init_buffer, init_buffer, init_buffer, init_buffer};
                
                for (int32_t x = 0; x < xmax; ++x) {
                    const uint8_t* src_ptr = im_in.ptr<uint8_t>(yy + offset) + (x + xmin) * ss1;
                    double coeff = k[x];
                    
                    // Optimized SIMD processing with vectorized coefficient multiplication
                    v128_t pixels = wasm_i32x4_make(src_ptr[0], src_ptr[1], 
                                                   ss1 > 2 ? src_ptr[2] : 0, 
                                                   ss1 > 3 ? src_ptr[3] : 0);
                    
                    // Convert pixels to float and multiply all at once
                    v128_t pixels_f1 = wasm_f64x2_convert_low_i32x4(pixels);
                    v128_t pixels_f2 = wasm_f64x2_make(
                        ss1 > 2 ? static_cast<double>(src_ptr[2]) : 0.0,
                        ss1 > 3 ? static_cast<double>(src_ptr[3]) : 0.0
                    );
                    v128_t coeff_vec = wasm_f64x2_splat(coeff);
                    
                    v128_t weighted1 = wasm_f64x2_mul(pixels_f1, coeff_vec);
                    v128_t weighted2 = wasm_f64x2_mul(pixels_f2, coeff_vec);
                    
                    // Accumulate results
                    results[0] += wasm_f64x2_extract_lane(weighted1, 0);
                    results[1] += wasm_f64x2_extract_lane(weighted1, 1);
                    if (ss1 > 2) results[2] += wasm_f64x2_extract_lane(weighted2, 0);
                    if (ss1 > 3) results[3] += wasm_f64x2_extract_lane(weighted2, 1);
                }
                
                // Clip and store results using optimized SIMD clip function
                uint8_t* dst_ptr = im_out.ptr<uint8_t>(yy) + xx * ss1;
                clip8_simd(results, dst_ptr, ss1);
            } else {
                // Fallback to scalar processing for other channel counts
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
}
#endif

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

#if HAVE_WASM_SIMD
// SIMD-optimized vertical resampling for uint8_t
void resampleVerticalSIMD(SimpleImage& im_out,
                         const SimpleImage& im_in,
                         int32_t offset,
                         int32_t ksize,
                         const std::vector<int32_t>& bounds,
                         const std::vector<double>& kk) {
    const int32_t ss0 = im_in.cols();
    const int32_t ss1 = im_in.channels();
    
    constexpr uint32_t precision_bits = 32 - 8 - 2;
    const double init_buffer = static_cast<double>(1U << (precision_bits - 1U));
    
    for (int32_t yy = 0; yy < im_out.rows(); ++yy) {
        int32_t ymin = bounds[yy * 2 + 0];
        int32_t ymax = bounds[yy * 2 + 1];
        const double* k = &kk[yy * ksize];
        
        for (int32_t xx = 0; xx < im_out.cols(); ++xx) {
            // For RGB/RGBA channels, process with optimized code
            if (ss1 == 3 || ss1 == 4) {
                // Accumulate results for all channels
                double results[4] = {init_buffer, init_buffer, init_buffer, init_buffer};
                
                for (int32_t y = 0; y < ymax; ++y) {
                    const uint8_t* src_ptr = im_in.ptr<uint8_t>(y + ymin) + xx * ss1;
                    double coeff = k[y];
                    
                    // Optimized SIMD processing with vectorized coefficient multiplication
                    v128_t pixels = wasm_i32x4_make(src_ptr[0], src_ptr[1], 
                                                   ss1 > 2 ? src_ptr[2] : 0, 
                                                   ss1 > 3 ? src_ptr[3] : 0);
                    
                    // Convert pixels to float and multiply all at once
                    v128_t pixels_f1 = wasm_f64x2_convert_low_i32x4(pixels);
                    v128_t pixels_f2 = wasm_f64x2_make(
                        ss1 > 2 ? static_cast<double>(src_ptr[2]) : 0.0,
                        ss1 > 3 ? static_cast<double>(src_ptr[3]) : 0.0
                    );
                    v128_t coeff_vec = wasm_f64x2_splat(coeff);
                    
                    v128_t weighted1 = wasm_f64x2_mul(pixels_f1, coeff_vec);
                    v128_t weighted2 = wasm_f64x2_mul(pixels_f2, coeff_vec);
                    
                    // Accumulate results
                    results[0] += wasm_f64x2_extract_lane(weighted1, 0);
                    results[1] += wasm_f64x2_extract_lane(weighted1, 1);
                    if (ss1 > 2) results[2] += wasm_f64x2_extract_lane(weighted2, 0);
                    if (ss1 > 3) results[3] += wasm_f64x2_extract_lane(weighted2, 1);
                }
                
                // Clip and store results using optimized SIMD clip function
                uint8_t* dst_ptr = im_out.ptr<uint8_t>(yy) + xx * ss1;
                clip8_simd(results, dst_ptr, ss1);
            } else {
                // Fallback to scalar processing for other channel counts
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
}
#endif

// Transpose function for SimpleImage with SIMD optimization
SimpleImage transpose(const SimpleImage& src) {
    if (src.empty()) return SimpleImage();
    
    SimpleImage dst(src.cols(), src.rows(), src.channels());
    
    const int channels = src.channels();
    
    // SIMD-optimized transpose for RGB/RGBA images
    if (channels == 3 || channels == 4) {
        for (int y = 0; y < src.rows(); y++) {
            for (int x = 0; x < src.cols(); x++) {
                const uint8_t* src_ptr = src.ptr<uint8_t>(y) + x * channels;
                uint8_t* dst_ptr = dst.ptr<uint8_t>(x) + y * channels;
                
                // Use SIMD to copy pixel data
                v128_t pixel_data = wasm_i32x4_make(
                    src_ptr[0], src_ptr[1], 
                    channels > 2 ? src_ptr[2] : 0,
                    channels > 3 ? src_ptr[3] : 0
                );
                
                dst_ptr[0] = static_cast<uint8_t>(wasm_i32x4_extract_lane(pixel_data, 0));
                dst_ptr[1] = static_cast<uint8_t>(wasm_i32x4_extract_lane(pixel_data, 1));
                if (channels > 2) dst_ptr[2] = static_cast<uint8_t>(wasm_i32x4_extract_lane(pixel_data, 2));
                if (channels > 3) dst_ptr[3] = static_cast<uint8_t>(wasm_i32x4_extract_lane(pixel_data, 3));
            }
        }
    } else {
        // Fallback to scalar transpose for other channel counts
        for (int y = 0; y < src.rows(); y++) {
            for (int x = 0; x < src.cols(); x++) {
                for (int c = 0; c < channels; c++) {
                    const uint8_t* src_ptr = src.ptr<uint8_t>(y) + x * channels + c;
                    uint8_t* dst_ptr = dst.ptr<uint8_t>(x) + y * channels + c;
                    *dst_ptr = *src_ptr;
                }
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
#if HAVE_WASM_SIMD
            resampleHorizontalSIMD(im_temp, src, ybox_first, ksize_horiz, bounds_horiz, kk_horiz);
#else
            resampleHorizontal<uint8_t>(im_temp, src, ybox_first, ksize_horiz, bounds_horiz, kk_horiz);
#endif
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
#if HAVE_WASM_SIMD
                resampleVerticalSIMD(im_out, im_temp, 0, ksize_vert, bounds_vert, kk_vert);
#else
                resampleVertical<uint8_t>(im_out, im_temp, 0, ksize_vert, bounds_vert, kk_vert);
#endif
            } else {
                throw std::runtime_error("Failed to allocate output image");
            }
        } else {
            // Use original image for vertical-only resize
            im_out.create(y_size, x_size, src.channels());
            if (!im_out.empty()) {
#if HAVE_WASM_SIMD
                resampleVerticalSIMD(im_out, src, 0, ksize_vert, bounds_vert, kk_vert);
#else
                resampleVertical<uint8_t>(im_out, src, 0, ksize_vert, bounds_vert, kk_vert);
#endif
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
