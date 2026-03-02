#!/bin/bash
# build-tvterm.sh
crt_dir=$(pwd);
build_log=$crt_dir/build-log.txt;

if [ -d build ]; then
  rm -R build;
fi

mkdir build
cd build
cmake .. \
  -DTV_BUILD_EXAMPLES=ON         \
  -DTV_BUILD_TESTS=ON            \
  -DBUILD_SHARED_LIBS=OFF        \
  -DBUILD_STATIC_LIBS=ON         \
  -DCMAKE_BUILD_TYPE=MinSizeRel  \
  -DTVTERM_OPTIMIZE_BUILD=ON     \
  2>&1 | tee $build_log

cmake --build . \
  2>&1 | tee -a $build_log

cd ..
if [ -f ./build/tvterm ]; then
  mkdir bin
  mv ./build/tvterm ./bin/tvterm-dinamic
  ls -la ./bin/
fi

# build static
cd build
bash ../build-tvterm-static.sh

cd ..
if [ -f ./build/tvterm ]; then
  mkdir bin
  mv ./build/tvterm ./bin/tvterm-static
  ls -la ./bin/
fi
