# CMake script for libvterm originally created by Rui Abreu Ferreira
# (https://github.com/equalsraf/libvterm-win).

cmake_minimum_required(VERSION 3.5)
project(libvterm LANGUAGES C)

include(GNUInstallDirs)
find_package(Perl REQUIRED)

set(VTERM_DIR ${CMAKE_CURRENT_LIST_DIR}/libvterm)

# Patches for libvterm (e.g. win32-input-mode). They are applied to a copy of
# the sources in the build directory, so the submodule itself stays unmodified.
file(GLOB VTERM_PATCHES ${CMAKE_CURRENT_LIST_DIR}/patches/*.patch)
list(SORT VTERM_PATCHES)
if (VTERM_PATCHES)
    find_program(PATCH_EXECUTABLE patch)
    if (NOT PATCH_EXECUTABLE)
        message(FATAL_ERROR "The 'patch' program is required to patch libvterm. "
                            "Install it (e.g. 'apt install patch' or 'apk add patch').")
    endif()
    set(VTERM_PATCHED_DIR ${CMAKE_CURRENT_BINARY_DIR}/libvterm-patched)
    file(REMOVE_RECURSE ${VTERM_PATCHED_DIR})
    file(COPY ${VTERM_DIR}/src ${VTERM_DIR}/include ${VTERM_DIR}/tbl2inc_c.pl
         DESTINATION ${VTERM_PATCHED_DIR})
    foreach (patch_file ${VTERM_PATCHES})
        get_filename_component(patch_name ${patch_file} NAME)
        execute_process(
            COMMAND ${PATCH_EXECUTABLE} -p1 --forward --input ${patch_file}
            WORKING_DIRECTORY ${VTERM_PATCHED_DIR}
            RESULT_VARIABLE patch_result
            OUTPUT_VARIABLE patch_output
            ERROR_VARIABLE patch_output
        )
        if (NOT patch_result EQUAL 0)
            message(FATAL_ERROR "Failed to apply libvterm patch '${patch_name}':\n${patch_output}")
        endif()
        message(STATUS "Applied libvterm patch: ${patch_name}")
    endforeach()
    # Re-run CMake when a patch changes.
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${VTERM_PATCHES})
    set(VTERM_DIR ${VTERM_PATCHED_DIR})
endif()

# Generate includes from tables
file(GLOB TBL_FILES ${VTERM_DIR}/src/encoding/*.tbl)
set(TBL_FILES_HEADERS)
foreach(file ${TBL_FILES})
    get_filename_component(basename ${file} NAME_WE)
    set(tname encoding/${basename}.inc)
    add_custom_command(
        COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_BINARY_DIR}/encoding/
        COMMAND ${PERL_EXECUTABLE} -CSD ${VTERM_DIR}/tbl2inc_c.pl ${file} > ${CMAKE_CURRENT_BINARY_DIR}/${tname}
        COMMENT "Generating ${tname}"
        OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/${tname}
    )
    list(APPEND TBL_FILES_HEADERS ${CMAKE_CURRENT_BINARY_DIR}/${tname})
endforeach()

file(GLOB VTERM_SOURCES ${VTERM_DIR}/src/*.c)
add_library(vterm ${VTERM_SOURCES} ${TBL_FILES_HEADERS})
target_include_directories(vterm PUBLIC
    "$<BUILD_INTERFACE:${VTERM_DIR}/include>"
    "$<INSTALL_INTERFACE:include>"
)
target_include_directories(vterm PRIVATE
    ${CMAKE_CURRENT_BINARY_DIR}
)

if(MSVC)
    target_compile_definitions(vterm PRIVATE
        _CRT_SECURE_NO_WARNINGS
        _CRT_NONSTDC_NO_DEPRECATE
    )
else()
    set_property(TARGET vterm PROPERTY C_STANDARD 99)
    set_property(TARGET vterm PROPERTY C_STANDARD_REQUIRED ON)
endif()
