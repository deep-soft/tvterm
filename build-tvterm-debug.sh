#!/bin/bash
# build-tvterm-debug.sh
crt_dir=$(pwd);
build_log=$crt_dir/build-log-debug.txt;
build_fld="build_debug";

if [ -d $build_fld ]; then
  echo  rm -R $build_fld;
fi

mkdir $build_fld;
cd $build_fld;

echo ">> BUILD BEGIN:"  2>&1 | tee $build_log;

cmake .. \
  -DTV_BUILD_EXAMPLES=ON         \
  -DTV_BUILD_TESTS=ON            \
  -DBUILD_SHARED_LIBS=OFF        \
  -DBUILD_STATIC_LIBS=ON         \
  -DCMAKE_BUILD_TYPE=Debug       \
  -DTVTERM_OPTIMIZE_BUILD=ON     \
  2>&1 | tee -a $build_log;

cmake --build . \
  2>&1 | tee -a $build_log;

cd ..;
if [ -f ./$build_fld/tvterm ]; then
  mkdir bin;
  mv ./$build_fld/tvterm ./bin/tvterm-dinamic-debug;
  ls -la ./bin/  2>&1 | tee -a $build_log;
fi

# build static
cd $build_fld;
bash ../build-tvterm-static-debug.sh $build_log;

cd ..;
if [ -f ./$build_fld/tvterm ]; then
  mkdir bin;
  mv ./$build_fld/tvterm ./bin/tvterm-static-debug;
  ls -la ./bin/  2>&1 | tee -a $build_log;
fi

echo ">> BUILD END:"  2>&1 | tee -a $build_log;
