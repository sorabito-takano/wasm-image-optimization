<template>
  <div class="image-card">
    <div class="image-container" @click="$emit('thumbnail-click')">
      <!-- Loading spinner -->
      <div v-if="image.isProcessing" class="loading-overlay">
        <div class="spinner"></div>
      </div>
      
      <!-- Error overlay -->
      <div v-else-if="image.error" class="error-overlay">
        <div class="error-icon">⚠️</div>
        <div class="error-text">{{ image.error }}</div>
      </div>
      
      <!-- Image preview -->
      <img
        v-else
        :src="image.processedUrl || image.originalUrl"
        :alt="image.name"
        class="image-preview"
        loading="lazy"
      >
      
      <!-- Status indicator -->
      <div class="status-indicator" :class="statusClass">
        {{ statusText }}
      </div>
    </div>
    
    <div class="card-content">
      <h4 class="image-name" :title="image.name">
        {{ image.name }}
      </h4>
      
      <div class="image-info">
        <div class="size-info">
          <span class="original-size">
            Original: {{ formatSize(image.originalSize) }}
          </span>
          <span v-if="image.processedSize" class="processed-size">
            → {{ formatSize(image.processedSize) }}
          </span>
        </div>
        
        <div v-if="image.processedSize" class="compression-info">
          Compression Ratio: {{ compressionRatio }}
        </div>
      </div>
      
      <button
        @click="$emit('remove')"
        class="remove-button"
        title="Remove image"
      >
        ✕
      </button>
    </div>
  </div>
</template>

<script setup lang="ts">
import type { ProcessedImage } from '~/types'

interface ImageCardProps {
  image: ProcessedImage
}

interface ImageCardEmits {
  'thumbnail-click': []
  'remove': []
}

const props = defineProps<ImageCardProps>()
const emit = defineEmits<ImageCardEmits>()

const { formatFileSize, getCompressionRatio } = useFileHandler()

const formatSize = (bytes: number) => formatFileSize(bytes)

const compressionRatio = computed(() => {
  return getCompressionRatio(props.image.originalSize, props.image.processedSize)
})

const statusClass = computed(() => {
  if (props.image.isProcessing) return 'processing'
  if (props.image.error) return 'error'
  if (props.image.processedData) return 'completed'
  return 'pending'
})

const statusText = computed(() => {
  if (props.image.isProcessing) return 'Processing...'
  if (props.image.error) return 'Error'
  if (props.image.processedData) return 'Done'
  return 'Pending'
})
</script>

<style scoped>
.image-card {
  background: white;
  border-radius: 12px;
  box-shadow: 0 2px 8px rgba(0, 0, 0, 0.1);
  overflow: hidden;
  transition: transform 0.2s ease, box-shadow 0.2s ease;
  position: relative;
}

.image-card:hover {
  transform: translateY(-2px);
  box-shadow: 0 4px 16px rgba(0, 0, 0, 0.15);
}

.image-container {
  position: relative;
  width: 200px;
  height: 200px;
  cursor: pointer;
  overflow: hidden;
  background: #f3f4f6;
  display: flex;
  align-items: center;
  justify-content: center;
}

.image-preview {
  width: 100%;
  height: 100%;
  object-fit: contain;
  display: block;
}

.loading-overlay {
  position: absolute;
  top: 0;
  left: 0;
  right: 0;
  bottom: 0;
  background: rgba(255, 255, 255, 0.9);
  display: flex;
  align-items: center;
  justify-content: center;
  z-index: 1;
}

.spinner {
  width: 40px;
  height: 40px;
  border: 3px solid #e5e7eb;
  border-top: 3px solid #4f46e5;
  border-radius: 50%;
  animation: spin 1s linear infinite;
}

@keyframes spin {
  0% { transform: rotate(0deg); }
  100% { transform: rotate(360deg); }
}

.error-overlay {
  position: absolute;
  top: 0;
  left: 0;
  right: 0;
  bottom: 0;
  background: rgba(254, 242, 242, 0.95);
  display: flex;
  flex-direction: column;
  align-items: center;
  justify-content: center;
  z-index: 1;
  padding: 1rem;
}

.error-icon {
  font-size: 2rem;
  margin-bottom: 0.5rem;
}

.error-text {
  font-size: 0.75rem;
  color: #dc2626;
  text-align: center;
}

.status-indicator {
  position: absolute;
  top: 8px;
  right: 8px;
  padding: 0.25rem 0.5rem;
  border-radius: 6px;
  font-size: 0.75rem;
  font-weight: 500;
  color: white;
}

.status-indicator.processing {
  background: #f59e0b;
}

.status-indicator.completed {
  background: #10b981;
}

.status-indicator.error {
  background: #dc2626;
}

.status-indicator.pending {
  background: #6b7280;
}

.card-content {
  padding: 1rem;
  position: relative;
}

.image-name {
  margin: 0 0 0.75rem;
  font-size: 0.875rem;
  font-weight: 600;
  color: #374151;
  white-space: nowrap;
  overflow: hidden;
  text-overflow: ellipsis;
  padding-right: 2rem;
}

.image-info {
  font-size: 0.75rem;
  color: #6b7280;
  line-height: 1.5;
}

.size-info {
  margin-bottom: 0.25rem;
}

.processed-size {
  color: #10b981;
  font-weight: 500;
}

.compression-info {
  font-weight: 500;
  color: #059669;
}

.remove-button {
  position: absolute;
  top: 0.5rem;
  right: 0.5rem;
  width: 1.5rem;
  height: 1.5rem;
  border: none;
  background: rgba(0, 0, 0, 0.1);
  color: #6b7280;
  border-radius: 50%;
  cursor: pointer;
  display: flex;
  align-items: center;
  justify-content: center;
  font-size: 0.75rem;
  transition: all 0.2s ease;
  opacity: 0;
}

.image-card:hover .remove-button {
  opacity: 1;
}

.remove-button:hover {
  background: #dc2626;
  color: white;
}
</style>