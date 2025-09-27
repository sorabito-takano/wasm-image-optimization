<template>
  <div class="demo-page">
    <header class="page-header">
      <h1>WASM Image Reducer Demo</h1>
  <p>High-performance image compression demo powered by WebAssembly (Nuxt 3 + TypeScript)</p>
  <p class="resize-note">Note: Images are resized so that the longer side is at most 1536px before compression.</p>
    </header>

    <main class="main-content">
      <!-- File Upload Section -->
      <section class="upload-section">
        <FileUpload :onFilesSelected="handleFilesSelected" />
      </section>

      <!-- Stats Section -->
      <section v-if="stats.total > 0" class="stats-section">
        <div class="stats-grid">
          <div class="stat-item">
            <div class="stat-number">{{ stats.total }}</div>
            <div class="stat-label">Total Files</div>
          </div>
          <div class="stat-item">
            <div class="stat-number">{{ stats.processing }}</div>
            <div class="stat-label">Processing</div>
          </div>
          <div class="stat-item">
            <div class="stat-number">{{ stats.completed }}</div>
            <div class="stat-label">Completed</div>
          </div>
          <div class="stat-item">
            <div class="stat-number">{{ stats.errors }}</div>
            <div class="stat-label">Errors</div>
          </div>
        </div>
        
        <div v-if="stats.completed > 0" class="compression-stats">
          <div class="compression-item">
            <span>Original Size:</span>
            <span>{{ formatFileSize(stats.originalTotalSize) }}</span>
          </div>
          <div class="compression-item">
            <span>Compressed:</span>
            <span>{{ formatFileSize(stats.processedTotalSize) }}</span>
          </div>
          <div class="compression-item highlight">
            <span>Reduction:</span>
            <span>{{ stats.totalCompressionRatio }}</span>
          </div>
        </div>
      </section>

      <!-- Actions Section -->
      <section v-if="images.length > 0" class="actions-section">
        <button
          @click="processAllImages"
          :disabled="isProcessing || allProcessed"
          class="action-button primary"
        >
          <span v-if="isProcessing">Processing...</span>
          <span v-else-if="allProcessed">All Done</span>
          <span v-else>Compress All (WebP, 80%, 1536x1536 - content)</span>
        </button>
        
        <button
          @click="clearAllImages"
          class="action-button secondary"
        >
          Clear
        </button>
      </section>

      <!-- Images Grid -->
      <section v-if="images.length > 0" class="images-section">
        <div class="images-grid">
          <ImageCard
            v-for="image in images"
            :key="image.id"
            :image="image"
            @thumbnail-click="openModal(image)"
            @remove="removeImage(image.id)"
          />
        </div>
      </section>

      <!-- Empty State -->
      <section v-else class="empty-state">
        <div class="empty-icon">🖼️</div>
  <h3>Upload images to get started</h3>
  <p>Supports JPEG, PNG, WebP, and GIF formats</p>
      </section>

      <!-- Client-side Only Notice -->
      <section class="info-section">
        <div class="info-box">
          <h4>🔧 Technical Info</h4>
          <div class="tech-details">
            <div v-if="isClient">
              <p>✅ Running on client side</p>
              <p>🔧 Web Workers: {{ workerCount }}</p>
              <p>📦 WASM: High-speed processing via WebAssembly</p>
            </div>
            <div v-else>
              <p>🔄 SSR mode - waiting for client hydration</p>
            </div>
          </div>
        </div>
      </section>
    </main>

    <!-- Image Modal -->
    <ImageModal
      :show="showModal"
      :image="selectedImage"
      @close="closeModal"
    />
  </div>
</template>

<script setup lang="ts">
import type { ProcessedImage } from '~/types'
import { setLimit, launchWorker } from '@sorabito-takano/wasm-image-optimization/web-worker';

setLimit(8); // Web Worker limit
launchWorker(); // Prepare Worker in advance.

// Composables
const { images, addFiles, removeImage, clearImages, formatFileSize, getStats } = useFileHandler()
const { processImages, initializeProcessor } = useImageProcessor()

// Reactive state
const showModal = ref(false)
const selectedImage = ref<ProcessedImage | null>(null)
const isProcessing = ref(false)
const isClient = ref(false)
const workerCount = ref(8)

// Computed properties
const stats = computed(() => getStats.value)

const allProcessed = computed(() => {
  return images.value.length > 0 && 
         images.value.every((img: ProcessedImage) => img.processedData || img.error)
})

// Methods
const handleFilesSelected = (files: File[]) => {
  addFiles(files)
}

const processAllImages = async () => {
  if (isProcessing.value || allProcessed.value) return
  
  isProcessing.value = true
  try {
    await processImages(images.value as ProcessedImage[])
  } catch (error) {
    console.error('Processing failed:', error)
  } finally {
    isProcessing.value = false
  }
}

const clearAllImages = () => {
  clearImages()
  closeModal()
}

const openModal = (image: ProcessedImage) => {
  selectedImage.value = image
  showModal.value = true
}

const closeModal = () => {
  showModal.value = false
  selectedImage.value = null
}

// Lifecycle
onMounted(async () => {
  isClient.value = true
})

// SEO
useHead({
  title: 'WASM Image Reducer Demo - Nuxt 3',
  meta: [
    { name: 'description', content: 'WebAssembly powered image compression demo built with Nuxt 3 and TypeScript' }
  ]
})
</script>

<style scoped>
.demo-page {
  max-width: 1200px;
  margin: 0 auto;
  padding: 2rem 1rem;
  min-height: 100vh;
}

.page-header {
  text-align: center;
  margin-bottom: 3rem;
}

.page-header h1 {
  font-size: 2.5rem;
  margin-bottom: 0.5rem;
  color: #1f2937;
}

.page-header p {
  color: #6b7280;
  font-size: 1.1rem;
}

.main-content {
  display: flex;
  flex-direction: column;
  gap: 2rem;
}

.upload-section {
  margin-bottom: 1rem;
}

.stats-section {
  background: white;
  border-radius: 12px;
  padding: 1.5rem;
  box-shadow: 0 2px 8px rgba(0, 0, 0, 0.1);
}

.stats-grid {
  display: grid;
  grid-template-columns: repeat(auto-fit, minmax(150px, 1fr));
  gap: 1rem;
  margin-bottom: 1.5rem;
}

.stat-item {
  text-align: center;
  padding: 1rem;
  background: #f8fafc;
  border-radius: 8px;
}

.stat-number {
  font-size: 2rem;
  font-weight: bold;
  color: #4f46e5;
  margin-bottom: 0.25rem;
}

.stat-label {
  font-size: 0.875rem;
  color: #6b7280;
}

.compression-stats {
  display: grid;
  grid-template-columns: repeat(auto-fit, minmax(200px, 1fr));
  gap: 1rem;
  padding-top: 1rem;
  border-top: 1px solid #e5e7eb;
}

.compression-item {
  display: flex;
  justify-content: space-between;
  align-items: center;
  padding: 0.5rem 0;
  font-size: 0.875rem;
}

.compression-item.highlight {
  font-weight: bold;
  color: #059669;
  font-size: 1rem;
}

.actions-section {
  display: flex;
  gap: 1rem;
  justify-content: center;
  flex-wrap: wrap;
}

.action-button {
  padding: 0.75rem 2rem;
  border: none;
  border-radius: 8px;
  font-weight: 600;
  cursor: pointer;
  transition: all 0.2s ease;
  font-size: 1rem;
}

.action-button:disabled {
  opacity: 0.6;
  cursor: not-allowed;
}

.action-button.primary {
  background: #4f46e5;
  color: white;
}

.action-button.primary:hover:not(:disabled) {
  background: #4338ca;
}

.action-button.secondary {
  background: #e5e7eb;
  color: #374151;
}

.action-button.secondary:hover {
  background: #d1d5db;
}

.images-section {
  margin-top: 1rem;
}

.images-grid {
  display: grid;
  grid-template-columns: repeat(auto-fill, minmax(200px, 1fr));
  gap: 1.5rem;
}

.empty-state {
  text-align: center;
  padding: 4rem 2rem;
  color: #6b7280;
}

.empty-icon {
  font-size: 4rem;
  margin-bottom: 1rem;
}

.empty-state h3 {
  margin-bottom: 0.5rem;
  color: #374151;
}

.info-section {
  margin-top: 2rem;
}

.info-box {
  background: #f0f4ff;
  border: 1px solid #c7d2fe;
  border-radius: 8px;
  padding: 1.5rem;
}

.info-box h4 {
  margin: 0 0 1rem;
  color: #312e81;
}

.tech-details {
  font-size: 0.875rem;
  color: #4338ca;
}

.tech-details p {
  margin: 0.25rem 0;
}

@media (max-width: 768px) {
  .demo-page {
    padding: 1rem;
  }
  
  .page-header h1 {
    font-size: 2rem;
  }
  
  .actions-section {
    flex-direction: column;
  }
  
  .action-button {
    width: 100%;
  }
  
  .images-grid {
    grid-template-columns: repeat(auto-fill, minmax(180px, 1fr));
    gap: 1rem;
  }
}
</style>