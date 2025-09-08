#!/bin/bash

# Architecture detection and Docker build script
# Usage: ./scripts/docker-build.sh [target]

# Helper function to run commands with fallback (continue on failure)
run_with_fallback() {
    local cmd="$1"
    local fallback_msg="$2"
    
    if eval "$cmd" 2>/dev/null; then
        return 0
    else
        echo "⚠️  $fallback_msg"
        return 1
    fi
}

# Execute critical commands (exit script on failure)
run_critical() {
    local cmd="$1"
    local error_msg="$2"
    
    if eval "$cmd"; then
        return 0
    else
        echo "❌ $error_msg"
        exit 1
    fi
}

# Move to project root relative to script directory
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
cd "$PROJECT_ROOT"

# Architecture detection
ARCH=$(uname -m)
echo "🔍 Detected architecture: $ARCH"

# Dockerfile selection (relative path from project root)
if [ "$ARCH" = "arm64" ] || [ "$ARCH" = "aarch64" ]; then
    DOCKERFILE="docker/Dockerfile.arm64"
    echo "🚀 Using ARM64 optimized Dockerfile"
else
    DOCKERFILE="docker/Dockerfile"
    echo "🚀 Using x86_64 Dockerfile"
fi

# Target configuration (default: all)
TARGET=${1:-all}

echo "📦 Building with:"
echo "   Project Root: $PROJECT_ROOT"
echo "   Dockerfile: $DOCKERFILE"
echo "   Target: $TARGET"
echo "   Architecture: $ARCH"

# Docker Composeでのビルド実行
export DOCKERFILE="$DOCKERFILE"

case "$TARGET" in
    "build-env")
        echo "🔧 Building build environment..."
        run_critical "docker compose -f docker/docker-compose.auto.yml build build-env" "Failed to build environment"
        echo "✅ Build environment created successfully!"
        ;;
    "dev")
        echo "🛠️  Starting development environment..."
        run_with_fallback "docker image rm wasm-image-optimization:runtime" "Image not found, continuing..."
        run_critical "docker compose -f docker/docker-compose.auto.yml up -d dev --build" "Failed to start development environment"
        echo "✅ Development environment started successfully!"
        ;;
    "make")
        echo "⚙️  Running make in Docker..."
        export DOCKERFILE="$DOCKERFILE"
        run_critical "docker compose -f docker/docker-compose.auto.yml run --rm dev make all" "Make command failed"
        echo "✅ Make completed successfully!"
        ;;
    "shell")
        echo "🐚 Opening shell in development environment..."
        export DOCKERFILE="$DOCKERFILE"
        run_critical "docker compose -f docker/docker-compose.auto.yml run --rm dev bash" "Failed to open shell"
        ;;
    "clean")
        echo "🧹 Cleaning up Docker resources..."
        run_with_fallback "docker compose -f docker/docker-compose.auto.yml down" "Compose down failed, continuing..."
        run_with_fallback "docker system prune -f" "System prune failed, continuing..."
        echo "✅ Cleanup completed!"
        ;;
    "all"|"")
        echo "🚀 Full build process..."
        export DOCKERFILE="$DOCKERFILE"
        run_critical "docker compose -f docker/docker-compose.auto.yml build" "Failed to build images"
        run_critical "docker compose -f docker/docker-compose.auto.yml run --rm dev make all" "Make command failed"
        echo "✅ Full build completed successfully!"
        ;;
    *)
        echo "❌ Unknown target: $TARGET"
        echo "Available targets: build-env, dev, make, shell, clean, all"
        exit 1
        ;;
esac

echo "✅ Build completed successfully!"
