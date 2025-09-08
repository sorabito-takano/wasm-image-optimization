import LibImage, { type ModuleType } from "../cjs/libImage.js";
import WASM from "../esm/libImage.wasm";
import { _optimizeImage, _optimizeImageExt } from "../lib/optimizeImage.js";
import { getWasmConfig, setWasmUrl, setWasmBinary, resetWasmConfig } from "../types/index.js";
import type { OptimizeParams, OptimizeResult, WasmConfig } from "../types/index.js";
export type { OptimizeParams, OptimizeResult, WasmConfig };
export { setWasmUrl, setWasmBinary, resetWasmConfig };

let libImageInstance: Promise<ModuleType> | null = null;

const getLibImage = (): Promise<ModuleType> => {
  if (!libImageInstance) {
    const config = getWasmConfig();
    
    if (config.wasmBinary) {
      // Use pre-loaded binary
      libImageInstance = LibImage({
        wasmBinary: config.wasmBinary,
      });
    } else if (config.wasmUrl) {
      // Use custom URL
      libImageInstance = LibImage({
        locateFile: () => config.wasmUrl!,
      });
    } else {
      // Use default bundled WASM
      libImageInstance = LibImage({
        instantiateWasm: async (imports, receiver) => {
          receiver(await WebAssembly.instantiate(WASM, imports));
        },
      });
    }
  }
  return libImageInstance!;
};

export const optimizeImage = async (params: OptimizeParams) =>
  _optimizeImage({ ...params, libImage: getLibImage() });

export const optimizeImageExt = async (params: OptimizeParams) =>
  _optimizeImageExt({ ...params, libImage: getLibImage() });
