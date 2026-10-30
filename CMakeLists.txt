cmake_minimum_required(VERSION 3.5...3.30)

option(TVTERM_BUILD_APP "Build main application" ON)
option(TVTERM_USE_SYSTEM_TVISION "Use system-wide Turbo Vision instead of the submodule" OFF)
option(TVTERM_USE_SYSTEM_LIBVTERM "Use system-wide libvterm instead of the submodule" OFF)
option(TVTERM_OPTIMIZE_BUILD "Enable build optimizations (Unity Build, Precompiled Headers)" ON)
option(TVTERM_STATIC "Build a fully static executable (Linux only)" OFF)

project(tvterm)

# Version shown by 'tvterm --version' and in More... > About. The commit,
# branch and build date are added automatically (scripts/buildinfo.cmake).
set(TVTERM_VERSION "0.1.0")

if (TVTERM_STATIC)
    # Make every find_library() call (here and in Turbo Vision) pick static
    # archives (.a) instead of shared libraries.
    set(CMAKE_FIND_LIBRARY_SUFFIXES ".a")
    message(STATUS "Building a static executable")
    include(deps/static-pthread.cmake)
endif()

function(tvterm_set_warnings t)
    if (NOT ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "MSVC"))
        target_compile_options(${t} PRIVATE
            -Wall
        )
    endif()
endfunction()

# Dependencies

include(deps/CMakeLists.txt)

# Target 'tvterm-core'

file(GLOB_RECURSE TVTERM_CORE_SRC "${CMAKE_CURRENT_LIST_DIR}/source/tvterm-core/*.cc")
add_library(tvterm-core ${TVTERM_CORE_SRC})

tvterm_set_warnings(tvterm-core)
if (HAVE_VTERMSTRINGFRAGMENT)
    target_compile_definitions(tvterm-core PRIVATE HAVE_VTERMSTRINGFRAGMENT)
endif()
if (NOT TVTERM_USE_SYSTEM_LIBVTERM)
    # The bundled libvterm is patched with win32-input-mode support
    # (see deps/vterm/patches).
    target_compile_definitions(tvterm-core PRIVATE HAVE_VTERM_WIN32INPUT)
endif()
target_include_directories(tvterm-core PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/include>"
    "$<INSTALL_INTERFACE:include>"
)
target_link_libraries(tvterm-core PUBLIC
    tvision
    vterm
    ${SYSTEM_DEPS}
)
install(TARGETS vterm ${SYSTEM_DEPS} EXPORT tvterm-config)
install(TARGETS tvterm-core
    EXPORT tvterm-config
    ARCHIVE DESTINATION lib
    COMPONENT library
)
install(EXPORT tvterm-config
    DESTINATION lib/cmake/tvterm
    NAMESPACE tvterm::
    FILE tvterm-config.cmake
    COMPONENT library
)
install(DIRECTORY "${CMAKE_CURRENT_LIST_DIR}/include/tvterm" DESTINATION include)

# Target 'tvterm'

if (TVTERM_BUILD_APP)
    file(GLOB_RECURSE TVTERM_SRC "${CMAKE_CURRENT_LIST_DIR}/source/tvterm/*.cc")

    # Build information (version, commit, date), generated at every build.
    set(TVTERM_BUILDINFO "${CMAKE_CURRENT_BINARY_DIR}/buildinfo.cc")
    add_custom_target(tvterm-buildinfo
        COMMAND "${CMAKE_COMMAND}"
            "-DSRC=${CMAKE_CURRENT_LIST_DIR}"
            "-DOUT=${TVTERM_BUILDINFO}"
            "-DVERSION=${TVTERM_VERSION}"
            "-DSTATIC=${TVTERM_STATIC}"
            "-DSYSTEM_TVISION=${TVTERM_USE_SYSTEM_TVISION}"
            "-DSYSTEM_LIBVTERM=${TVTERM_USE_SYSTEM_LIBVTERM}"
            -P "${CMAKE_CURRENT_LIST_DIR}/scripts/buildinfo.cmake"
        BYPRODUCTS "${TVTERM_BUILDINFO}"
        VERBATIM
    )
    # A small file that changes often: kept out of the unity build and
    # the precompiled headers, so that only it is compiled again.
    set_source_files_properties("${TVTERM_BUILDINFO}" PROPERTIES
        GENERATED ON
        SKIP_UNITY_BUILD_INCLUSION ON
        SKIP_PRECOMPILE_HEADERS ON
    )

    add_executable(tvterm ${TVTERM_SRC} "${TVTERM_BUILDINFO}")
    add_dependencies(tvterm tvterm-buildinfo)
    tvterm_set_warnings(tvterm)
    target_link_libraries(tvterm PRIVATE
        tvterm-core
    )
    if (TVTERM_STATIC)
        target_link_libraries(tvterm PRIVATE -static)
    endif()
    install(TARGETS tvterm RUNTIME DESTINATION bin)
endif()

# Build optimization

if (${CMAKE_VERSION} VERSION_GREATER_EQUAL "3.16.0")
    if (TVTERM_OPTIMIZE_BUILD)
        foreach (t tvterm tvterm-core)
            set_target_properties(${t} PROPERTIES UNITY_BUILD ON)
            target_precompile_headers(${t} PRIVATE
                "${CMAKE_CURRENT_LIST_DIR}/include/pch.h"
            )
        endforeach()
    endif()
endif()
