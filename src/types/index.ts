export type OptimizeResult = {
  data: Uint8Array;
  originalWidth: number;
  originalHeight: number;
  width: number;
  height: number;
};

export type OptimizeParams = {
  image: BufferSource | string; // The input image data
  width?: number; // The desired output width (optional)
  height?: number; // The desired output height (optional)
  quality?: number; // The desired output quality (0-100, optional)
  format?: "webp" | "none"; // The desired output format - WebP only (optional)
};

export type WasmConfig = {
  wasmUrl?: string; // Custom URL for libImage.wasm
  wasmBinary?: ArrayBuffer; // Pre-loaded WASM binary
};

// Global configuration
let globalWasmConfig: WasmConfig = {};

export const setWasmUrl = (url: string) => {
  globalWasmConfig.wasmUrl = url;
};

export const setWasmBinary = (binary: ArrayBuffer) => {
  globalWasmConfig.wasmBinary = binary;
};

export const getWasmConfig = (): WasmConfig => ({ ...globalWasmConfig });

export const resetWasmConfig = () => {
  globalWasmConfig = {};
};
