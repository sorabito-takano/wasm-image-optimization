# wasm-image-optimization

WebAssembly-based image optimization library with a **custom minimal resize core** (OpenCV runtime removed) using extracted high-quality Lanczos resampling logic from [pillow-resize](https://github.com/zurutech/pillow-resize). Primary target is **WebP** (auto lossless for PNG/WebP inputs, lossy otherwise) with optional **JPEG** output and a **pass-through ("none")** mode that returns the original bytes (useful when only resizing info or EXIF-based orientation handling is needed).

- Frontend

  - Next.js (Multithreading support)
  - React Router (Multithreading support)

- Backend

  - Cloudflare Workers
  - Deno Deploy
  - Node.js (Multithreading support)

## Supported Conversions

- Input formats (auto-detected): **JPEG / PNG / WebP**
- Output formats:
  - `webp`  – High-quality Lanczos resize using [pillow-resize](https://github.com/zurutech/pillow-resize) implementation, auto lossless for PNG/WebP sources, lossy otherwise
  - `jpeg`  – Always lossy JPEG (RGB → YCbCr), ignores lossless flag
  - `none`  – Returns original bytes untouched (width/height/EXIF orientation still processed)

## Example

https://next-image-convert.vercel.app/  
![](https://raw.githubusercontent.com/node-libraries/wasm-image-optimization/refs/heads/master/doc/image.webp)

## API

### Core Functions

Image conversion (all functions return Promises):

```ts
optimizeImage({
  image: ArrayBuffer | Uint8Array | string,
  width?: number,
  height?: number,
  quality?: number,   // 0-100 (default 100)
  format?: "webp" | "jpeg" | "none" // default: webp
}): Promise<Uint8Array>

optimizeImageExt({
  image: ArrayBuffer | Uint8Array | string,
  width?: number,
  height?: number,
  quality?: number,
  format?: "webp" | "jpeg" | "none"
}): Promise<{
  data: Uint8Array,
  originalWidth: number,
  originalHeight: number,
  width: number,
  height: number
}>

```

### Multi-thread / Worker Control

```ts
waitAll(): Promise<void>
waitReady(retryTime?: number): Promise<void>
close(): void
launchWorker(): Promise<void>
setLimit(maxWorkers: number): void   // (exported but previously undocumented)
```

### WASM Path Configuration (PWA / Offline Support)

For PWA applications or custom deployment scenarios, you can configure the WebAssembly binary location:

```ts
import { setWasmUrl, setWasmBinary } from 'wasm-image-optimization/vite';

// Option 1: Set custom WASM URL (useful for PWA Service Worker caching)
setWasmUrl('/assets/libImage.wasm');

// Option 2: Provide pre-loaded WASM binary
const wasmBinary = await fetch('/assets/libImage.wasm').then(r => r.arrayBuffer());
setWasmBinary(wasmBinary);

// Reset to default behavior
import { resetWasmConfig } from 'wasm-image-optimization/vite';
resetWasmConfig();
```

Available in: `vite`, `next`, `workers`, `node` entry points.

## Vite / Web Worker Integration

For Vite usage include the plugin (adds proper asset copying and worker wiring):

- vite.config.ts

```ts
import wasmImageOptimizationPlugin from "wasm-image-optimization/vite-plugin";

export default defineConfig(() => ({
  plugins: [
    wasmImageOptimizationPlugin(),
    //wasmImageOptimizationPlugin("build/client/assets") // optional: assetsPath
  ],
  optimizeDeps: {
    //...
    exclude: [
      //...
      '@sorabito-takano/wasm-image-optimization'
    ]
  },
  ssr: {
    //...
    noExternal: [
      //...
      '@sorabito-takano/wasm-image-optimization'
    ]
  }
}));
```

## Build

### Architecture-aware Docker Build

Automatic host architecture detection (x86_64 / arm64) is handled by the helper script:

```bash
# Automatic architecture detection and build
pnpm build:wasm:auto

# Or use the architecture detection script directly
./scripts/docker-build.sh all

# Manual architecture selection (override detection)
pnpm docker:arm64    # Force ARM64 build
pnpm docker:x86      # Force x86_64 build
```

### Incremental Development (Docker)

```bash
# Build environment setup
pnpm docker:build-env

# Start development environment
pnpm docker:dev

# Run make in Docker
pnpm docker:make

# Open shell in Docker
pnpm docker:shell-dev

# Clean up Docker resources  
pnpm docker:stop
```

### Direct One-shot Build (manual override)

```bash
# For ARM64 (Apple Silicon)
DOCKERFILE=./docker/Dockerfile.arm64 docker compose -f docker/docker-compose.auto.yml run --rm dev make all

# For x86_64
DOCKERFILE=./docker/Dockerfile docker compose -f docker/docker-compose.auto.yml run --rm dev make all
```

## Supported Environments & Entry Points

| Environment / Use Case                | Import Path                                        |
|---------------------------------------|----------------------------------------------------|
| Cloudflare Workers / Edge (ESM)       | `wasm-image-optimization`                          |
| Next.js (SSR + Web Worker mode)       | `wasm-image-optimization/next`                     |
| Next.js API Route (explicit)          | `wasm-image-optimization/next-api`                 |
| Generic ESM / Deno Deploy             | `wasm-image-optimization`                          |
| Node.js (single thread)               | `wasm-image-optimization`                          |
| Node.js (multi thread pool)           | `wasm-image-optimization/node-worker`              |
| Vite (bundled browser main thread)    | `wasm-image-optimization/vite`                     |
| Vite / Generic Web Worker (multi)     | `wasm-image-optimization/web-worker`               |
| Raw Worker (CJS fallback)             | `wasm-image-optimization/node`                     |

> Multi-thread variants expose `waitReady`, `waitAll`, `launchWorker`, `setLimit`, `close`.

## Samples

Repository with usage examples:
https://github.com/SoraKumo001/wasm-image-optimization-samples

## Behavior Notes

- EXIF orientation is automatically normalized before resizing/encoding.
- WebP encoding switches to **lossless** when input is PNG or WebP and output format is `webp`.
- `format: "none"` returns the original bytes (useful when you only need metadata or want to defer encoding).
- `quality` only affects lossy paths (WebP lossy / JPEG). Lossless WebP ignores the numeric quality parameter.

## Image Processing Details

This library integrates high-quality image resampling algorithms extracted from [pillow-resize](https://github.com/zurutech/pillow-resize) for superior image quality compared to typical naive or bilinear scaling approaches. **OpenCV is no longer shipped** to drastically reduce WASM size.

### Minimal Resize Core & Lanczos Resampling

| Aspect | Before (OpenCV based) | Now (Custom minimal core) |
|--------|-----------------------|---------------------------|
| WASM size (typical) | ~1.5 MB | ~ (significantly smaller)* |
| Dependency | OpenCV (core/imgproc subset) | Hand-extracted resize pipeline |
| Filter | OpenCV Lanczos / fallback logic | Direct Lanczos-3 implementation |
| Fallback path | OpenCV INTER_LANCZOS4 | Removed (deterministic path) |

*Exact size depends on build flags; OpenCV code paths, alloc helpers, and unused kernels removed.

#### Key Points
- Only the math & buffer ops needed for Lanczos-3 down/upsampling are compiled.
- No dynamic dispatch / no unused interpolation kernels.
- Deterministic single path (no runtime fallback → smaller + predictable output).
- Implements horizontal + vertical separable filtering with windowed sinc (Lanczos radius=3).
- Designed for future extension (e.g. optional Mitchell / Catmull-Rom) without pulling large frameworks.

#### Why remove OpenCV?
- Large binary footprint (initial builds ~1.5MB → unacceptable for some edge/PWA budgets)
- Only resize + color conversion were used.
- Custom path eliminates build complexity (Python + CMake toolchain for OpenCV JS).
- Faster cold start under Service Worker / edge runtime due to reduced instantiation cost.

#### Quality
Lanczos-3 maintains sharpness while minimizing ringing for photographic sources. For heavy downscales (<25%) you may still apply an external pre-blur if aggressive aliasing in source exists.

> If you previously relied on OpenCV-specific behavior (e.g. other interpolation modes), migrate by calling `optimizeImage` with the same resize parameters — behavior is now unified.

## Roadmap / TODO

- Expose pluggable resize kernels (Mitchell, Catmull-Rom, Lanczos-2).
- Optional AVIF output (investigation phase).
- SIMD tuned inner loops for Lanczos (current version already uses -msimd128 at compile but kernel still scalar in places).
- Update published TypeScript types to include `"jpeg"` (if not already updated in release at read time).
- Optional prefilter for extreme downscale scenarios.

---
MIT License
