#include "simple_imgproc.h"
#include <algorithm>

namespace simple_imgproc {

void cvtColor(const cv::Mat& src, cv::Mat& dst, ColorConversion conversion) {
    int rows = src.rows;
    int cols = src.cols;
    
    switch (conversion) {
        case RGB2BGR:
        case BGR2RGB: {
            // RGB <-> BGR swap (same operation)
            if (src.channels() != 3) return;
            
            dst.create(rows, cols, CV_8UC3);
            
            for (int i = 0; i < rows; i++) {
                const uint8_t* src_row = src.ptr<uint8_t>(i);
                uint8_t* dst_row = dst.ptr<uint8_t>(i);
                
                for (int j = 0; j < cols; j++) {
                    int idx = j * 3;
                    dst_row[idx] = src_row[idx + 2];     // R <-> B
                    dst_row[idx + 1] = src_row[idx + 1]; // G stays
                    dst_row[idx + 2] = src_row[idx];     // B <-> R
                }
            }
            break;
        }
        
        case RGBA2BGR: {
            if (src.channels() != 4) return;
            
            dst.create(rows, cols, CV_8UC3);
            
            for (int i = 0; i < rows; i++) {
                const uint8_t* src_row = src.ptr<uint8_t>(i);
                uint8_t* dst_row = dst.ptr<uint8_t>(i);
                
                for (int j = 0; j < cols; j++) {
                    int src_idx = j * 4;
                    int dst_idx = j * 3;
                    dst_row[dst_idx] = src_row[src_idx + 2];     // R -> B
                    dst_row[dst_idx + 1] = src_row[src_idx + 1]; // G -> G
                    dst_row[dst_idx + 2] = src_row[src_idx];     // B -> R
                    // Alpha channel is discarded
                }
            }
            break;
        }
        
        case GRAY2BGR: {
            if (src.channels() != 1) return;
            
            dst.create(rows, cols, CV_8UC3);
            
            for (int i = 0; i < rows; i++) {
                const uint8_t* src_row = src.ptr<uint8_t>(i);
                uint8_t* dst_row = dst.ptr<uint8_t>(i);
                
                for (int j = 0; j < cols; j++) {
                    uint8_t gray_val = src_row[j];
                    int idx = j * 3;
                    dst_row[idx] = gray_val;     // B
                    dst_row[idx + 1] = gray_val; // G
                    dst_row[idx + 2] = gray_val; // R
                }
            }
            break;
        }
    }
}

void rotate(const cv::Mat& src, cv::Mat& dst, RotationType rotation) {
    int src_rows = src.rows;
    int src_cols = src.cols;
    int channels = src.channels();
    
    switch (rotation) {
        case ROTATE_90_CLOCKWISE: {
            dst.create(src_cols, src_rows, src.type());
            
            for (int i = 0; i < src_rows; i++) {
                const uint8_t* src_row = src.ptr<uint8_t>(i);
                for (int j = 0; j < src_cols; j++) {
                    uint8_t* dst_pixel = dst.ptr<uint8_t>(j) + (src_rows - 1 - i) * channels;
                    const uint8_t* src_pixel = src_row + j * channels;
                    
                    for (int c = 0; c < channels; c++) {
                        dst_pixel[c] = src_pixel[c];
                    }
                }
            }
            break;
        }
        
        case ROTATE_180: {
            dst.create(src_rows, src_cols, src.type());
            
            for (int i = 0; i < src_rows; i++) {
                const uint8_t* src_row = src.ptr<uint8_t>(i);
                uint8_t* dst_row = dst.ptr<uint8_t>(src_rows - 1 - i);
                
                for (int j = 0; j < src_cols; j++) {
                    const uint8_t* src_pixel = src_row + j * channels;
                    uint8_t* dst_pixel = dst_row + (src_cols - 1 - j) * channels;
                    
                    for (int c = 0; c < channels; c++) {
                        dst_pixel[c] = src_pixel[c];
                    }
                }
            }
            break;
        }
        
        case ROTATE_90_COUNTERCLOCKWISE: {
            dst.create(src_cols, src_rows, src.type());
            
            for (int i = 0; i < src_rows; i++) {
                const uint8_t* src_row = src.ptr<uint8_t>(i);
                for (int j = 0; j < src_cols; j++) {
                    uint8_t* dst_pixel = dst.ptr<uint8_t>(src_cols - 1 - j) + i * channels;
                    const uint8_t* src_pixel = src_row + j * channels;
                    
                    for (int c = 0; c < channels; c++) {
                        dst_pixel[c] = src_pixel[c];
                    }
                }
            }
            break;
        }
    }
}

} // namespace simple_imgproc