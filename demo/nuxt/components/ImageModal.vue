<template>
  <Teleport to="body">
    <div
      v-if="show && image"
      class="modal-overlay"
      @click="handleOverlayClick"
    >
      <div class="modal-content" @click.stop>
        <div class="modal-header">
          <h3>{{ image.name }}</h3>
          <button @click="$emit('close')" class="close-button">
            ✕
          </button>
        </div>
        
        <div class="modal-body">
          <div class="image-wrapper">
            <img
              :src="image.processedUrl || image.originalUrl"
              :alt="image.name"
              class="full-image"
            >
          </div>
        </div>
        
        <div class="modal-footer">
          <div class="image-details">
            <div class="detail-item">
              <span class="label">Original Size:</span>
              <span class="value">{{ formatSize(image.originalSize) }}</span>
            </div>
            <div v-if="image.processedSize" class="detail-item">
              <span class="label">Compressed:</span>
              <span class="value">{{ formatSize(image.processedSize) }}</span>
            </div>
            <div v-if="image.processedSize" class="detail-item">
              <span class="label">Compression Ratio:</span>
              <span class="value">{{ compressionRatio }}</span>
            </div>
            <div class="detail-item">
              <span class="label">Format:</span>
              <span class="value">{{ image.processedData ? 'WebP' : getFileType(image.originalFile.type) }}</span>
            </div>
          </div>
        </div>
      </div>
    </div>
  </Teleport>
</template>

<script setup lang="ts">
import type { ProcessedImage } from '~/types'

interface ImageModalProps {
  show: boolean
  image: ProcessedImage | null
}

interface ImageModalEmits {
  close: []
}

const props = defineProps<ImageModalProps>()
const emit = defineEmits<ImageModalEmits>()

const { formatFileSize, getCompressionRatio } = useFileHandler()

const formatSize = (bytes: number) => formatFileSize(bytes)

const compressionRatio = computed(() => {
  if (!props.image) return '-'
  return getCompressionRatio(props.image.originalSize, props.image.processedSize)
})

const getFileType = (mimeType: string): string => {
  const typeMap: Record<string, string> = {
    'image/jpeg': 'JPEG',
    'image/png': 'PNG',
    'image/gif': 'GIF',
    'image/webp': 'WebP',
    'image/bmp': 'BMP',
    'image/svg+xml': 'SVG'
  }
  return typeMap[mimeType] || 'Unknown'
}

const handleOverlayClick = () => {
  emit('close')
}

// Close modal on Escape key
onMounted(() => {
  const handleKeydown = (event: KeyboardEvent) => {
    if (event.key === 'Escape' && props.show) {
      emit('close')
    }
  }
  
  document.addEventListener('keydown', handleKeydown)
  
  onUnmounted(() => {
    document.removeEventListener('keydown', handleKeydown)
  })
})
</script>

<style scoped>
.modal-overlay {
  position: fixed;
  top: 0;
  left: 0;
  right: 0;
  bottom: 0;
  background: rgba(0, 0, 0, 0.8);
  display: flex;
  align-items: center;
  justify-content: center;
  z-index: 1000;
  padding: 2rem;
}

.modal-content {
  background: white;
  border-radius: 12px;
  max-width: 90vw;
  max-height: 90vh;
  display: flex;
  flex-direction: column;
  overflow: hidden;
  box-shadow: 0 20px 40px rgba(0, 0, 0, 0.3);
}

.modal-header {
  display: flex;
  justify-content: space-between;
  align-items: center;
  padding: 1.5rem;
  border-bottom: 1px solid #e5e7eb;
  background: #f9fafb;
}

.modal-header h3 {
  margin: 0;
  font-size: 1.25rem;
  color: #374151;
  flex: 1;
  white-space: nowrap;
  overflow: hidden;
  text-overflow: ellipsis;
  margin-right: 1rem;
}

.close-button {
  background: none;
  border: none;
  font-size: 1.5rem;
  color: #6b7280;
  cursor: pointer;
  padding: 0.25rem;
  border-radius: 4px;
  transition: all 0.2s ease;
  width: 2rem;
  height: 2rem;
  display: flex;
  align-items: center;
  justify-content: center;
}

.close-button:hover {
  background: #e5e7eb;
  color: #374151;
}

.modal-body {
  flex: 1;
  overflow: auto;
  display: flex;
  align-items: center;
  justify-content: center;
  padding: 1rem;
  min-height: 0;
}

.image-wrapper {
  display: flex;
  align-items: center;
  justify-content: center;
  width: 100%;
  height: 100%;
}

.full-image {
  max-width: 100%;
  max-height: 70vh;
  object-fit: contain;
  border-radius: 8px;
}

.modal-footer {
  padding: 1.5rem;
  border-top: 1px solid #e5e7eb;
  background: #f9fafb;
}

.image-details {
  display: grid;
  grid-template-columns: repeat(auto-fit, minmax(200px, 1fr));
  gap: 1rem;
}

.detail-item {
  display: flex;
  justify-content: space-between;
  align-items: center;
  padding: 0.5rem 0;
}

.label {
  font-weight: 500;
  color: #6b7280;
}

.value {
  color: #374151;
  font-weight: 600;
}

@media (max-width: 768px) {
  .modal-overlay {
    padding: 1rem;
  }
  
  .image-details {
    grid-template-columns: 1fr;
  }
  
  .full-image {
    max-height: 60vh;
  }
}
</style>