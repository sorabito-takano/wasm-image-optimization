# wasm-image-optimization

WebAssembly-based image optimization library with a **custom minimal resize core** (OpenCV runtime removed for smaller wasm size) using extracted high-quality Lanczos resampling logic from [pillow-resize](https://github.com/zurutech/pillow-resize). Primary target is **WebP** (auto lossless for PNG/WebP inputs, lossy otherwise) with optional **JPEG** output and a **pass-through ("none")** mode that returns the original bytes (useful when only resizing info or EXIF-based orientation handling is needed).

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

https://wasm-image-optimization.web.app

sources are at demo/nuxt

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

### WASM Optimization and Reproducibility

The Docker builds pin Emscripten **5.0.7**, libwebp **1.6.0**, and libexif
**0.6.26**. JPEG/PNG ports are supplied by that Emscripten version. OpenCV is
not built or linked. The default is `-Oz`, SIMD, and `emmalloc`; LTO remains
opt-in because it increased uncompressed WASM size in the comparison below.
Use `MALLOC=dlmalloc` to select the previous allocator.

Measurements on macOS arm64, Homebrew Emscripten `5.0.7-git`, Node **24.15.0**:

| Configuration | WASM bytes | gzip bytes | Brotli bytes | Sum of case medians (ms) |
| --- | ---: | ---: | ---: | ---: |
| Baseline: `-Oz`, dlmalloc | 706,655 | 295,565 | 246,114 | 1,832.4 |
| LTO, dlmalloc | 713,009 | 294,101 | 244,509 | 1,868.7 |
| LTO, emmalloc | 706,110 | 291,840 | 242,383 | 1,867.7 |
| **Selected: `-Oz`, emmalloc** | **699,854** | **293,147** | **243,808** | **1,836.8** |

The selected configuration saves **6,801 bytes (0.96%)** uncompressed and
**2,306 bytes (0.94%)** with Brotli. Timings are indicative, not evidence of a
speedup: they include the JS call, output copy, validation, and hashing.
All configurations use the same updated dependencies and SIMD boundary fixes;
this is not a comparison against the old published/demo binary.

The benchmark used five local images (including two untracked JPEGs with/without
EXIF), plus generated 1x1, 4x1, and 5x1 RGB PNGs. All 32 conversion cases matched
byte-for-byte across configurations, with one warm-up and three measured rounds.
The test also checks 16 short unsupported inputs. Linear memory capacity stayed
at 105,316,352 bytes after warm-up in all four builds; this is not a heap leak
analysis. ESM/CJS shared-WASM parity was separately checked for 15 cases.

Docker verification also passed on Colima **arm64**, Docker **29.2.1**, and
standalone Docker Compose **5.1.4**. Both Dockerfiles built successfully and
produced ESM/CJS artifacts using Emscripten **5.0.7**. The default Dockerfile
selected its ARM64 base image on this host; x86_64 execution remains untested.
Docker baseline/selected builds, the default-Dockerfile build, and the local
selected build passed the same 32-case output comparison and 16 short-input
checks. Docker-generated ESM/CJS shared-WASM parity passed all 15 cases.

| Docker configuration | WASM bytes | gzip bytes | Brotli bytes |
| --- | ---: | ---: | ---: |
| Baseline: dlmalloc | 706,655 | 296,187 | 245,542 |
| Selected: emmalloc (both Dockerfiles) | 699,854 | 293,768 | 243,886 |

Docker and local builds had identical image outputs, but their compressed WASM
sizes differed slightly. Browser/Worker integration remains untested.

To repeat the comparison, run these commands in a build environment with the
pinned, configured libwebp/libexif source directories (such as the Docker dev
shell). Use fresh output/work directories when changing flags or dependencies:

```bash
make -j4 esm WORKDIR=work/bench-baseline DISTDIR=dist/bench/baseline MALLOC=dlmalloc
make -j4 esm WORKDIR=work/bench-lto DISTDIR=dist/bench/lto MALLOC=dlmalloc EXTRA_CFLAGS=-flto
make -j4 esm WORKDIR=work/bench-lto-emmalloc DISTDIR=dist/bench/lto-emmalloc MALLOC=emmalloc EXTRA_CFLAGS=-flto
make -j4 esm WORKDIR=work/bench-emmalloc DISTDIR=dist/bench/emmalloc MALLOC=emmalloc
```

Then, on the host with Node **24+**, run:

```bash
node test/wasm-benchmark.mjs dist/bench/baseline/esm dist/bench/lto/esm dist/bench/lto-emmalloc/esm dist/bench/emmalloc/esm
```

The first directory is the reference. The script exits nonzero on an output
mismatch, conversion failure, or WASM trap, and prints a JSON report on success.
It reads JPEG/PNG/WebP files in `images/` without modifying them. No additional
npm dependencies are needed. `make clean` now removes dependency objects under
`WORKDIR`; old source-adjacent `.o` files are no longer used.

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

The helpers require the `docker compose` plugin. On the verified Colima host,
only standalone `docker-compose` was available and Buildx was absent, so the
following fallback was used without changing host Docker configuration:

```bash
DOCKER_BUILDKIT=0 DOCKERFILE=docker/Dockerfile.arm64 docker-compose -f docker/docker-compose.auto.yml build dev
DOCKERFILE=docker/Dockerfile.arm64 docker-compose -f docker/docker-compose.auto.yml run --rm --no-deps dev make -j4 all DISTDIR=dist/docker-check
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
- Update published TypeScript types to include `"jpeg"` (if not already updated in release at read time).
- Optional prefilter for extreme downscale scenarios.

---
MIT License
