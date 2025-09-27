import { setWorkerCount as setGlobalWorkerCount, getWorkerCount as getGlobalWorkerCount, processImage as processImageWithPool } from './utils/worker-pool.js';
/**
 * WebAssembly based image reducer with WebWorker support
 */
export class ImageReducer {
    isInitialized = false;
    /**
     * Constructor - creates lightweight instance that uses global worker pool
     */
    constructor() {
        // Lightweight constructor - no worker initialization here
    }
    /**
     * Set global worker count (static method)
     * This affects all ImageReducer instances
     */
    static setWorkerCount(count) {
        setGlobalWorkerCount(count);
    }
    /**
     * Get current global worker count (static method)
     */
    static getWorkerCount() {
        return getGlobalWorkerCount();
    }
    /**
     * Initialize the image reducer
     * This is optional - reduce() will auto-initialize if needed
     */
    async initialize() {
        if (this.isInitialized) {
            return;
        }
        // The actual initialization is handled by the global worker pool
        // This method is mainly for explicit initialization if needed
        this.isInitialized = true;
    }
    /**
     * Reduce (compress and resize) an image
     *
     * @param image - Input image as ArrayBuffer
     * @param params - Processing parameters
     * @returns Promise resolving to processed image result
     */
    async reduce(image, params) {
        // Auto-initialize if not already done
        if (!this.isInitialized) {
            await this.initialize();
        }
        // Validate inputs
        if (!image || image.byteLength === 0) {
            throw new Error('Invalid image: ArrayBuffer is empty or null');
        }
        if (!params.format || !['jpeg', 'webp'].includes(params.format)) {
            throw new Error('Invalid format: must be "jpeg" or "webp"');
        }
        if (typeof params.quality !== 'number' || params.quality < 0 || params.quality > 100) {
            throw new Error('Invalid quality: must be a number between 0 and 100');
        }
        if (params.width && (typeof params.width !== 'number' || params.width <= 0)) {
            throw new Error('Invalid width: must be a positive number');
        }
        if (params.height && (typeof params.height !== 'number' || params.height <= 0)) {
            throw new Error('Invalid height: must be a positive number');
        }
        try {
            // Process image using global worker pool
            const result = await processImageWithPool(image, params);
            return result;
        }
        catch (error) {
            console.error('Image reduction failed:', error);
            throw new Error(`Image reduction failed: ${error instanceof Error ? error.message : String(error)}`);
        }
    }
    /**
     * Check if the reducer is initialized
     */
    get initialized() {
        return this.isInitialized;
    }
}
// Export static methods as standalone functions for convenience
export const setWorkerCount = ImageReducer.setWorkerCount;
export const getWorkerCount = ImageReducer.getWorkerCount;
//# sourceMappingURL=index.js.map