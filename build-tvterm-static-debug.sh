#!/bin/bash
# build-tvterm-static-debug.sh

if [[ $1 != "" ]]; then
  build_log=$1;
else
  crt_dir=$(pwd);
  build_log=$crt_dir/build-log-debug.txt;
fi

kernel_version=$(uname -v);
echo "kernel_version=["$kernel_version"]" 2>&1 | tee -a $build_log;

usr_lib="none";
if [[ $kernel_version == *"Ubuntu"* ]]; then
  #ubuntu
  usr_lib='/usr/lib/x86_64-linux-gnu';
fi

if [[ $kernel_version == *"Alpine"* ]]; then
  #alpine
  usr_lib='/usr/lib';
fi
echo "usr_lib=["$usr_lib"]"  2>&1 | tee -a $build_log;

# #ubuntu
# usr_lib='/usr/lib/x86_64-linux-gnu';
# #alpine
# usr_lib='/usr/lib';

if [[ "$usr_lib" != "" ]]; then
  echo ">> BUILD STATIC BEGIN:"  2>&1 | tee -a $build_log;

  /usr/bin/c++ -v -Os \
  -static \
  -Wl,--dependency-file,CMakeFiles/tvterm.dir/link.d \
  CMakeFiles/tvterm.dir/Unity/unity_0_cxx.cxx.o \
  -o tvterm  \
  -static \
  libtvterm-core.a \
  deps/tvision/libtvision.a \
  deps/vterm/libvterm.a \
  $usr_lib/libgpm.a \
  $usr_lib/libncursesw.a \
  $usr_lib/libtinfo.a \
  $usr_lib/libutil.a \
  $usr_lib/libpthread.a \
  2>&1 | tee -a $build_log;

  echo ">> BUILD STATIC END:"  2>&1 | tee -a $build_log;
fi
