SHELL=/bin/bash
WORKDIR=work
DISTDIR=dist
ESMDIR=$(DISTDIR)/esm
WORKERSDIR=$(DISTDIR)/cjs

TARGET_ESM_BASE = $(notdir $(basename src/libImage.cpp))
TARGET_ESM = $(ESMDIR)/$(TARGET_ESM_BASE).js
TARGET_WORKERS = $(WORKERSDIR)/$(TARGET_ESM_BASE).js

CFLAGS = -O3 -msimd128 -sSTACK_SIZE=5MB \
        -Ilibwebp -Ilibwebp/src -Iopencv/include -Iopencv/modules/core/include \
        -Iopencv/modules/imgproc/include -Iopencv/modules/imgcodecs/include \
        -DOPENCV_WEBP_ONLY

CFLAGS_ASM = --bind \
             -s WASM=1 -s ALLOW_MEMORY_GROWTH=1 -s ENVIRONMENT=web -s DYNAMIC_EXECUTION=0 -s MODULARIZE=1

WEBP_SOURCES := $(wildcard libwebp/src/dsp/*.c) \
                $(wildcard libwebp/src/enc/*.c) \
                $(wildcard libwebp/src/utils/*.c) \
                $(wildcard libwebp/src/dec/*.c) \
                $(wildcard libwebp/sharpyuv/*.c)

OPENCV_SOURCES := opencv/modules/core/src/alloc.cpp \
                  opencv/modules/core/src/array.cpp \
                  opencv/modules/core/src/matrix.cpp \
                  opencv/modules/core/src/mat_ops.cpp \
                  opencv/modules/imgproc/src/resize.cpp \
                  opencv/modules/imgproc/src/imgwarp.cpp \
                  opencv/modules/imgproc/src/filter.cpp \
                  opencv/modules/imgcodecs/src/loadsave.cpp

WEBP_OBJECTS := $(WEBP_SOURCES:.c=.o)
OPENCV_OBJECTS := $(OPENCV_SOURCES:.cpp=.o)

.PHONY: all esm workers clean

all: esm workers

$(WEBP_OBJECTS): %.o: %.c
	@emcc $(CFLAGS) -c $< -o $@

$(OPENCV_OBJECTS): %.o: %.cpp
	@emcc $(CFLAGS) -c $< -o $@

$(WORKDIR):
	@mkdir -p $(WORKDIR)

$(WORKDIR)/webp.a: $(WORKDIR) $(WEBP_OBJECTS)
	@emar rcs $@ $(WEBP_OBJECTS)

$(WORKDIR)/opencv.a: $(WORKDIR) $(OPENCV_OBJECTS)
	@emar rcs $@ $(OPENCV_OBJECTS)

$(ESMDIR) $(WORKERSDIR):
	@mkdir -p $@

esm: $(TARGET_ESM)

$(TARGET_ESM): src/libImage.cpp $(WORKDIR)/webp.a $(WORKDIR)/opencv.a | $(ESMDIR)
	emcc $(CFLAGS) -o $@ $^ \
       $(CFLAGS_ASM)  -s EXPORT_ES6=1

workers: $(TARGET_WORKERS)

$(TARGET_WORKERS): src/libImage.cpp $(WORKDIR)/webp.a $(WORKDIR)/opencv.a | $(WORKERSDIR)
	emcc $(CFLAGS) -o $@ $^ \
       $(CFLAGS_ASM)
	@rm $(WORKERSDIR)/$(TARGET_ESM_BASE).wasm

clean:
	@echo Cleaning up...
	@rm -rf $(WORKDIR) $(ESMDIR) $(WORKERSDIR)
