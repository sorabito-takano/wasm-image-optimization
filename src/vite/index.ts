import LibImage, { type ModuleType } from "../cjs/libImage.js";
import WASM from "../esm/libImage.wasm?url";
import { _optimizeImage, _optimizeImageExt } from "../lib/optimizeImage.js";
import { getWasmConfig, setWasmUrl, setWasmBinary, resetWasmConfig } from "../types/index.js";
import type { OptimizeParams, OptimizeResult, WasmConfig } from "../types/index.js";
export type { OptimizeParams, OptimizeResult, WasmConfig };
export { setWasmUrl, setWasmBinary, resetWasmConfig };

let libImage: Promise<ModuleType>;
const getLibImage = async () => {
  if (!libImage) {
    const config = getWasmConfig();
    
    if (config.wasmBinary) {
      // Use pre-loaded binary
      libImage = LibImage({
        wasmBinary: config.wasmBinary,
      });
    } else if (config.wasmUrl) {
      // Use custom URL
      libImage = LibImage({
        wasmBinary: await fetch(config.wasmUrl).then((v) => v.arrayBuffer()),
      });
    } else {
      // Use default bundled WASM
      libImage = LibImage({
        wasmBinary: await fetch(WASM).then((v) => v.arrayBuffer()),
      });
    }
  }
  return libImage;
};

export const optimizeImage = (params: OptimizeParams) =>
  _optimizeImage({ ...params, libImage: getLibImage() });

export const optimizeImageExt = (params: OptimizeParams) =>
  _optimizeImageExt({ ...params, libImage: getLibImage() });
