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
