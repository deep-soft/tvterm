#!/bin/bash
# build-tvterm-static-v2.sh
crt_dir=$(pwd);
build_log=$crt_dir/build-log.txt;

if [ -d build ]; then
  rm -R build;
fi

mkdir build
cd build
cmake .. \
  -DCMAKE_CXX_FLAGS="-std=c++14 -static" \
  -DCMAKE_PROJECT_INCLUDE="$crt_dir/.github/workflows/find-static-libs.cmake" \
  -DTV_BUILD_EXAMPLES=OFF        \
  -DTV_BUILD_TESTS=OFF           \
  -DBUILD_SHARED_LIBS=OFF        \
  -DBUILD_STATIC_LIBS=ON         \
  -DCMAKE_BUILD_TYPE=MinSizeRel  \
  -DTVTERM_OPTIMIZE_BUILD=ON     \
  2>&1 | tee $build_log

cmake --build . \
  2>&1 | tee -a $build_log

