import type { ReduceParams, ReduceResult } from './types/index.js';
/**
 * WebAssembly based image reducer with WebWorker support
 */
export declare class ImageReducer {
    private isInitialized;
    /**
     * Constructor - creates lightweight instance that uses global worker pool
     */
    constructor();
    /**
     * Set global worker count (static method)
     * This affects all ImageReducer instances
     */
    static setWorkerCount(count: number): void;
    /**
     * Get current global worker count (static method)
     */
    static getWorkerCount(): number;
    /**
     * Initialize the image reducer
     * This is optional - reduce() will auto-initialize if needed
     */
    initialize(): Promise<void>;
    /**
     * Reduce (compress and resize) an image
     *
     * @param image - Input image as ArrayBuffer
     * @param params - Processing parameters
     * @returns Promise resolving to processed image result
     */
    reduce(image: ArrayBuffer, params: ReduceParams): Promise<ReduceResult>;
    /**
     * Check if the reducer is initialized
     */
    get initialized(): boolean;
}
export type { ReduceParams, ReduceResult } from './types/index.js';
export declare const setWorkerCount: typeof ImageReducer.setWorkerCount;
export declare const getWorkerCount: typeof ImageReducer.getWorkerCount;
//# sourceMappingURL=index.d.ts.map