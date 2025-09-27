import type { ProcessedImage } from '../types'

export const useFileHandler = () => {
  const images = ref<ProcessedImage[]>([])

  // Add files to the list
  const addFiles = (files: FileList | File[]) => {
    const fileArray = Array.from(files)
    
    fileArray.forEach(file => {
      // Validate file type
      if (!file.type.startsWith('image/')) {
        console.warn('Skipping non-image file:', file.name)
        return
      }

      // Create object URL for preview
      const originalUrl = URL.createObjectURL(file)

      // Create processed image object
      const processedImage: ProcessedImage = {
        id: `${Date.now()}-${Math.random().toString(36).substr(2, 9)}`,
        name: file.name,
        originalFile: file,
        originalSize: file.size,
        originalUrl,
        isProcessing: false
      }

      images.value.push(processedImage)
    })
  }

  // Remove a specific image
  const removeImage = (id: string) => {
    const index = images.value.findIndex((img: ProcessedImage) => img.id === id)
    if (index >= 0) {
      const image = images.value[index]
      
      // Clean up object URLs
      URL.revokeObjectURL(image.originalUrl)
      if (image.processedUrl) {
        URL.revokeObjectURL(image.processedUrl)
      }

      images.value.splice(index, 1)
    }
  }

  // Clear all images
  const clearImages = () => {
    // Clean up all object URLs
    images.value.forEach((image: ProcessedImage) => {
      URL.revokeObjectURL(image.originalUrl)
      if (image.processedUrl) {
        URL.revokeObjectURL(image.processedUrl)
      }
    })

    images.value = []
  }

  // Format file size
  const formatFileSize = (bytes: number): string => {
    if (bytes === 0) return '0 B'
    
    const k = 1024
    const sizes = ['B', 'KB', 'MB', 'GB']
    const i = Math.floor(Math.log(bytes) / Math.log(k))
    
    return parseFloat((bytes / Math.pow(k, i)).toFixed(2)) + ' ' + sizes[i]
  }

  // Calculate compression ratio
  const getCompressionRatio = (originalSize: number, processedSize?: number): string => {
    if (!processedSize) return '-'
    
    const ratio = ((originalSize - processedSize) / originalSize) * 100
    return ratio > 0 ? `${ratio.toFixed(1)}%` : '0%'
  }

  // Get processing statistics
  const getStats = computed(() => {
    const total = images.value.length
    const processing = images.value.filter((img: ProcessedImage) => img.isProcessing).length
    const completed = images.value.filter((img: ProcessedImage) => img.processedData).length
    const errors = images.value.filter((img: ProcessedImage) => img.error).length

    const originalTotalSize = images.value.reduce((sum: number, img: ProcessedImage) => sum + img.originalSize, 0)
    const processedTotalSize = images.value
      .filter((img: ProcessedImage) => img.processedSize)
      .reduce((sum: number, img: ProcessedImage) => sum + (img.processedSize || 0), 0)

    return {
      total,
      processing,
      completed,
      errors,
      originalTotalSize,
      processedTotalSize,
      totalCompressionRatio: getCompressionRatio(originalTotalSize, processedTotalSize)
    }
  })

  // Handle file drop
  const handleDrop = (event: DragEvent) => {
    event.preventDefault()
    
    if (event.dataTransfer?.files) {
      addFiles(event.dataTransfer.files)
    }
  }

  // Handle drag over
  const handleDragOver = (event: DragEvent) => {
    event.preventDefault()
  }

  return {
    images: readonly(images),
    addFiles,
    removeImage,
    clearImages,
    formatFileSize,
    getCompressionRatio,
    getStats,
    handleDrop,
    handleDragOver
  }
}