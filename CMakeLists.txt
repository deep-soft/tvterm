cmake_minimum_required(VERSION 3.5)

option(TVTERM_BUILD_APP "Build main application" ON)
option(TVTERM_USE_SYSTEM_TVISION "Use system-wide Turbo Vision instead of the submodule" OFF)
option(TVTERM_USE_SYSTEM_LIBVTERM "Use system-wide libvterm instead of the submodule" OFF)
option(TVTERM_OPTIMIZE_BUILD "Enable build optimizations (Unity Build, Precompiled Headers)" ON)
option(TV_BUILD_EXAMPLES "Build example apps" ON)
option(TV_BUILD_TESTS "Build tests" ON)

project(tvterm)

#set(CMAKE_C_FLAGS "-D__FLAT__")
#set(CMAKE_CXX_FLAGS "-D__FLAT__")

function(tvterm_set_warnings t)
    if (NOT ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "MSVC"))
        target_compile_options(${t} PRIVATE
            -Wall
# -D__FLAT__
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
    find_package(Curses REQUIRED)
    include_directories(${NCURSES_INCLUDE_DIR})
    include_directories(${NCURSES_INCLUDE_DIRS})

    set(BUILD_SHARED_LIBS OFF)
#    set(CMAKE_EXE_LINKER_FLAGS "-static")
    file(GLOB_RECURSE TVTERM_SRC "${CMAKE_CURRENT_LIST_DIR}/source/tvterm/*.cc")
#    link_libraries("-static")
    add_executable(tvterm ${TVTERM_SRC})

    tvterm_set_warnings(tvterm)
    target_include_directories(tvterm PRIVATE
        ${NCURSES_INCLUDE_DIR}
        ${NCURSES_INCLUDE_DIRS}
    )
    target_link_libraries(tvterm PRIVATE
        tvterm-core
        ${NCURSES_LIBRARIES}
    )

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
