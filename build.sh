#!/bin/bash
set -e

mkdir -p build
cd build

if [ ! -f "CMakeCache.txt" ]; then
    echo "No cache found, running cmake..."
    cmake .. -DPICO_SDK_PATH=$PICO_SDK_PATH
else
    echo "Using existing cmake cache..."
fi

make -j4
cd ..
echo "Build complete: build/tomogatchi.uf2"