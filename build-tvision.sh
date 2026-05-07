#!/bin/bash
# build-tvision.sh
crt_dir=$(pwd);
build_log=$crt_dir/build-tvision-log.txt;
cd deps/tvision/;

if [ -d build ]; then
  rm -R build;
fi

mkdir build;
cd build;
cmake .. \
  -DTV_BUILD_EXAMPLES=ON         \
  -DTV_BUILD_TESTS=ON            \
  -DBUILD_SHARED_LIBS=OFF        \
  -DBUILD_STATIC_LIBS=ON         \
  -DCMAKE_BUILD_TYPE=MinSizeRel  \
  2>&1 | tee $build_log;

cmake --build . \
  2>&1 | tee -a $build_log;
