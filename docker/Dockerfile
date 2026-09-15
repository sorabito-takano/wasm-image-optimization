FROM emscripten/emsdk:5.0.7 AS build-env
WORKDIR /app

ARG LIBWEBP_VERSION=1.6.0
ARG LIBEXIF_VERSION=0.6.26

RUN apt-get update && apt-get install -y --no-install-recommends git curl ca-certificates bzip2 && \
    rm -rf /var/lib/apt/lists/* && \
    git clone --depth 1 --branch "v${LIBWEBP_VERSION}" https://github.com/webmproject/libwebp && \
    curl --fail --location --retry 3 \
        "https://github.com/libexif/libexif/releases/download/v${LIBEXIF_VERSION}/libexif-${LIBEXIF_VERSION}.tar.bz2" \
        -o /tmp/libexif.tar.bz2 && \
    tar -xjf /tmp/libexif.tar.bz2 && \
    mv "libexif-${LIBEXIF_VERSION}" libexif && \
    rm /tmp/libexif.tar.bz2 && \
    cd libexif && \
    emconfigure ./configure --disable-shared --enable-static --disable-nls --disable-docs

FROM build-env AS runtime
WORKDIR /app
