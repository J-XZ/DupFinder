#!/usr/bin/bash

# 检查第一个参数是不是 "-f"
if [ "$1" == "-f" ]; then
    rm -rf build
    cd third_party/xxHash || exit
    make clean
    cd ../.. || exit
fi

cd third_party/xxHash || exit
XXH_INLINE_ALL=1 ccache make -j
cd ../.. || exit

mkdir -p build
cd build || exit
cmake .. -G Ninja \
    -DCMAKE_BUILD_TYPE=Debug \
    -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
    -DCMAKE_C_COMPILER_LAUNCHER=ccache \
    -DCMAKE_CXX_COMPILER_LAUNCHER=ccache
cmake --build . -- -j"$(nproc)"
cd .. || exit
