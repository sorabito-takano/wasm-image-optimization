<template>
  <div class="file-upload">
    <div
      class="upload-area"
      :class="{ 'drag-over': isDragOver }"
      @drop="handleDrop"
      @dragover="handleDragOver"
      @dragenter="handleDragEnter"
      @dragleave="handleDragLeave"
    >
      <div class="upload-content">
        <div class="upload-icon">
          📷
        </div>
  <h3>Select files or drag & drop</h3>
  <p>Supports JPEG, PNG, WebP, and GIF image files</p>
        
        <label class="file-input-label">
          <input
            type="file"
            multiple
            accept="image/*"
            @change="handleFileSelect"
            class="file-input"
          >
          <span class="file-input-button">Choose Files</span>
        </label>
      </div>
    </div>
  </div>
</template>

<script setup lang="ts">
interface FileUploadProps {
  onFilesSelected: (files: File[]) => void
}

const props = defineProps<FileUploadProps>()

const isDragOver = ref(false)

const handleFileSelect = (event: Event) => {
  const input = event.target as HTMLInputElement
  if (input.files) {
    props.onFilesSelected(Array.from(input.files))
  }
}

const handleDrop = (event: DragEvent) => {
  event.preventDefault()
  isDragOver.value = false
  
  if (event.dataTransfer?.files) {
    props.onFilesSelected(Array.from(event.dataTransfer.files))
  }
}

const handleDragOver = (event: DragEvent) => {
  event.preventDefault()
}

const handleDragEnter = (event: DragEvent) => {
  event.preventDefault()
  isDragOver.value = true
}

const handleDragLeave = (event: DragEvent) => {
  event.preventDefault()
  // Only set to false if leaving the actual drop zone
  const target = event.currentTarget as HTMLElement
  const relatedTarget = event.relatedTarget as HTMLElement
  if (target && relatedTarget && !target.contains(relatedTarget)) {
    isDragOver.value = false
  }
}
</script>

<style scoped>
.file-upload {
  margin-bottom: 2rem;
}

.upload-area {
  border: 2px dashed #cbd5e0;
  border-radius: 12px;
  padding: 3rem 2rem;
  text-align: center;
  transition: all 0.3s ease;
  background: #f8fafc;
}

.upload-area:hover,
.upload-area.drag-over {
  border-color: #4f46e5;
  background: #f0f4ff;
}

.upload-content h3 {
  margin: 1rem 0 0.5rem;
  font-size: 1.25rem;
  color: #374151;
}

.upload-content p {
  margin: 0 0 1.5rem;
  color: #6b7280;
  font-size: 0.875rem;
}

.upload-icon {
  font-size: 3rem;
  margin-bottom: 0.5rem;
}

.file-input-label {
  display: inline-block;
  cursor: pointer;
}

.file-input {
  display: none;
}

.file-input-button {
  display: inline-block;
  padding: 0.75rem 1.5rem;
  background: #4f46e5;
  color: white;
  border-radius: 8px;
  font-weight: 500;
  transition: background 0.2s ease;
}

.file-input-button:hover {
  background: #4338ca;
}
</style>