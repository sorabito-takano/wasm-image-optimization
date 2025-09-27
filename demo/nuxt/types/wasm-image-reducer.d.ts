// Type definitions for wasm-image-reducer package
declare module 'wasm-image-reducer' {
  export interface ProcessingOptions {
    quality?: number;
    format?: 'webp' | 'jpeg' | 'png';
    maxWidth?: number;
    maxHeight?: number;
  }

  export interface ProcessingResult {
    originalSize: number;
    compressedSize: number;
    compressionRatio: number;
    blob: Blob;
  }

  export function processImageInWorker(
    file: File,
    options?: ProcessingOptions
  ): Promise<ProcessingResult>;

  export function createWorkerInstance(): Worker;
}

// Global type declarations
declare global {
  interface Window {
    Worker: typeof Worker;
  }
}

export {};