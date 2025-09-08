#!/bin/bash

# 簡易アーキテクチャ検出とDockerビルド
# ARM64の場合は自動でDockerfile.arm64を使用

ARCH=$(uname -m)

if [ "$ARCH" = "arm64" ] || [ "$ARCH" = "aarch64" ]; then
    echo "🍎 ARM64 detected - using Dockerfile.arm64"
    export DOCKERFILE="./docker/Dockerfile.arm64"
else
    echo "🖥️  x86_64 detected - using default Dockerfile"
    export DOCKERFILE="./docker/Dockerfile"
fi

# 引数をそのまま docker compose に渡す
docker compose -f docker/docker-compose.auto.yml "$@"
