FROM emscripten/emsdk
WORKDIR /app
RUN  apt-get update && apt-get install -y dh-autoreconf ninja-build yasm python3-numpy &&\
    git clone https://github.com/webmproject/libwebp &&\
    git clone https://github.com/opencv/opencv &&\
    cd opencv && git checkout 4.10.0 &&\
    ln -s /app/libwebp/src/webp /emsdk/upstream/lib/clang/21/include/webp
