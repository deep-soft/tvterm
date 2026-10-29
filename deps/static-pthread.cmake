# Static builds with glibc older than 2.34.
#
# Before 2.34, glibc keeps the thread functions in a separate libpthread.a.
# A static link takes from it only the objects referenced directly, and
# std::thread then aborts at startup ("terminate called without an active
# exception"). Linking the whole archive avoids that.
# musl and glibc 2.34+ (libpthread merged into libc) are not affected.
#
# The result is cached in TVTERM_OLD_GLIBC; -DTVTERM_OLD_GLIBC=ON/OFF
# overrides the check.

include(CheckCSourceCompiles)

check_c_source_compiles("
#include <features.h>
#if !defined(__GLIBC__) || __GLIBC__ > 2 || (__GLIBC__ == 2 && __GLIBC_MINOR__ >= 34)
#error not an old glibc
#endif
int main(void) { return 0; }
" TVTERM_OLD_GLIBC)

if (TVTERM_OLD_GLIBC)
    set(TVTERM_WHOLE_PTHREAD "-Wl,--whole-archive -lpthread -Wl,--no-whole-archive")
    # The same option given twice would define every pthread symbol twice.
    string(FIND "${CMAKE_EXE_LINKER_FLAGS}" "--whole-archive -lpthread" TVTERM_PTHREAD_POS)
    if (TVTERM_PTHREAD_POS EQUAL -1)
        string(APPEND CMAKE_EXE_LINKER_FLAGS " ${TVTERM_WHOLE_PTHREAD}")
        message(STATUS "glibc older than 2.34: linking the whole libpthread")
    else()
        message(STATUS "glibc older than 2.34: whole libpthread already in CMAKE_EXE_LINKER_FLAGS")
    endif()
endif()
