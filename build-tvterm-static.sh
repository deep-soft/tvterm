/usr/bin/c++ -Os -DNDEBUG -D__FLAT__ \
-static \
-Wl,--dependency-file,CMakeFiles/tvterm.dir/link.d \
CMakeFiles/tvterm.dir/Unity/unity_0_cxx.cxx.o \
-o tvterm  \
-static \
libtvterm-core.a \
deps/tvision/libtvision.a \
/usr/lib/libgpm.a \
/usr/lib/libncursesw.a \
/usr/lib/libtinfo.a \
deps/vterm/libvterm.a \
/usr/lib/libutil.a \
/usr/lib/libpthread.a
