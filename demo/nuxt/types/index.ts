export interface ProcessedImage {
  id: string
  name: string
  originalFile: File
  originalSize: number
  originalUrl: string
  processedData?: Uint8Array
  processedSize?: number
  processedUrl?: string
  isProcessing: boolean
  error?: string
}

export interface ProcessingResult {
  success: boolean
  data?: Uint8Array
  originalSize: number
  processedSize?: number
  error?: string
}