import type { ProcessedImage, ProcessingResult } from '../types'
import { optimizeImageExt } from '@sorabito-takano/wasm-image-optimization/web-worker';

export const useImageProcessor = () => {
  let imageReducer: any = null

  // Initialize image reducer (client-side only)
  const initializeProcessor = async () => {
  }

  // Process a single image
  const processImage = async (file: File): Promise<ProcessingResult> => {
      try {
      // Convert File to ArrayBuffer
      const arrayBuffer = await file.arrayBuffer()

      // Process the image (WebP, quality 80, 800x600 max)
      const image = await optimizeImageExt({
        image: arrayBuffer,
        format: 'webp',
        quality: 80,
        width: 1536,
        height: 1536,
      });

      if (!image || !image.data) {
        throw new Error('Image processing failed: No data returned');
      }
      return {
        success: true,
        data: image.data,
        originalSize: arrayBuffer.byteLength,
        processedSize: image.data.length
      }
    } catch (error) {
      console.error('Image processing failed:', error)
      return {
        success: false,
        originalSize: file.size,
        error: error instanceof Error ? error.message : 'Unknown error'
      }
    }
  }

  // Process multiple images
  const processImages = async (images: ProcessedImage[]): Promise<void> => {
    if (typeof window === 'undefined') {
      return
    }

    const processingPromises = images
      .filter(img => !img.isProcessing && !img.processedData)
      .map(async (image, index) => {
        // Mark as processing
        image.isProcessing = true

        try {
          const result = await processImage(image.originalFile)
          
          if (result.success && result.data) {
            // Create blob URL for processed image
            console.log('result.data', result.data);
            const reduceImg = new Blob([new Uint8Array(result.data)], { type: 'image/webp' });
            const processedUrl = URL.createObjectURL(reduceImg)

            // Update the original image object directly
            Object.assign(image, {
              processedData: result.data,
              processedSize: result.processedSize,
              processedUrl,
              isProcessing: false,
              error: undefined
            })
          } else {
            Object.assign(image, {
              isProcessing: false,
              error: result.error || 'Processing failed'
            })
          }
        } catch (error) {
          Object.assign(image, {
            isProcessing: false,
            error: error instanceof Error ? error.message : 'Processing failed'
          })
        }
      })

    await Promise.all(processingPromises)
  }

  return {
    initializeProcessor,
    processImage,
    processImages
  }
}