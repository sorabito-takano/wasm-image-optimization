# wasm-image-optimization

WebAssembly-based image optimization library powered by OpenCV. Primary target is **WebP** (auto lossless for PNG/WebP inputs, lossy otherwise) with optional **JPEG** output and a **pass-through ("none")** mode that returns the original bytes (useful when only resizing info or EXIF-based orientation handling is needed).

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
  - `webp`  – Lanczos resize, auto lossless for PNG/WebP sources, lossy otherwise
  - `jpeg`  – Always lossy JPEG (RGB → YCbCr), ignores lossless flag
  - `none`  – Returns original bytes untouched (width/height/EXIF orientation still processed)

> NOTE: Current TypeScript type (`OptimizeParams.format`) lists `"webp" | "none"`. The native layer already supports `"jpeg"` and README reflects actual behavior. A future minor release will expand the published type to include `"jpeg"`.

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

## Roadmap / TODO

- Update published TypeScript types to include `"jpeg"` in `OptimizeParams.format`.
- Optional AVIF output (investigation phase).
- WASM size reduction via tree-shaken OpenCV custom build.

---
MIT License
