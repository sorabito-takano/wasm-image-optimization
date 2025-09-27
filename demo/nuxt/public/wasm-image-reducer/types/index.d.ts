/**
 * Parameters for image reduction
 */
export interface ReduceParams {
    /** Output format: 'jpeg' or 'webp' */
    format: 'jpeg' | 'webp';
    /** Compression quality (0-100) */
    quality: number;
    /** Target width in pixels */
    width?: number;
    /** Target height in pixels */
    height?: number;
}
/**
 * Result of image reduction
 */
export interface ReduceResult {
    /** Compressed image data */
    data: Uint8Array;
    /** Original image width */
    originalWidth: number;
    /** Original image height */
    originalHeight: number;
    /** Processed image width */
    width: number;
    /** Processed image height */
    height: number;
}
/**
 * Internal worker message types
 */
export interface WorkerMessage {
    type: 'reduce';
    payload: {
        image: ArrayBuffer;
        params: ReduceParams;
    };
}
export interface WorkerResponse {
    success: boolean;
    result?: ReduceResult;
    error?: string;
}
/**
 * LibImage module type (from reference)
 */
export interface ModuleType {
    optimize: (data: BufferSource | string, width: number, height: number, quality: number, format: "webp" | "jpeg" | "none") => ReduceResult | undefined;
    releaseResult: () => void;
}
/**
 * LibImage factory function
 */
export type LibImageFactory = (options?: {
    instantiateWasm?: (imports: WebAssembly.Imports, receiver: (instance: WebAssembly.WebAssemblyInstantiatedSource) => Promise<unknown>) => void;
    locateFile?: (path: string, scriptDirectory: string) => string;
    wasmBinary?: ArrayBuffer;
}) => Promise<ModuleType>;
//# sourceMappingURL=index.d.ts.map