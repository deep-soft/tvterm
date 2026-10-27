# Fixes for the bundled tvision, applied without modifying the submodule:
# the affected source file is copied to the build directory, changed there,
# and used instead of the original in the 'tvision' target.
#
# histlist.cpp: the history of input lines (the list next to the Name field
# of file dialogs) moves its entries with memcpy over overlapping memory
# (the DOS original used movmem). That is undefined behaviour; with
# _FORTIFY_SOURCE on musl (Alpine) memcpy stops the program with
# "Illegal instruction" when the ranges overlap, e.g. when a name already
# in the history is chosen again. memmove handles overlapping ranges.

set(_TV_SRC "${CMAKE_CURRENT_LIST_DIR}/tvision/source/tvision/histlist.cpp")
set(_TV_FIXED "${CMAKE_CURRENT_BINARY_DIR}/tvision-fixed/histlist.cpp")
file(READ "${_TV_SRC}" _tv_text)
set(_tv_new "${_tv_text}")
string(REPLACE "memcpy( curRec, n, " "memmove( curRec, n, " _tv_new "${_tv_new}")
string(REPLACE "memcpy( dst, src, " "memmove( dst, src, " _tv_new "${_tv_new}")
string(REGEX MATCHALL "memmove\\( (curRec, n|dst, src), " _tv_hits "${_tv_new}")
list(LENGTH _tv_hits _tv_count)
if (_tv_count EQUAL 2 AND NOT _tv_new STREQUAL _tv_text)
    file(WRITE "${_TV_FIXED}.tmp" "${_tv_new}")
    # Rewrite only when the content changes, so the file is not rebuilt each time.
    configure_file("${_TV_FIXED}.tmp" "${_TV_FIXED}" COPYONLY)
    get_target_property(_tv_sources tvision SOURCES)
    list(FIND _tv_sources "${_TV_SRC}" _tv_index)
    if (_tv_index EQUAL -1)
        message(WARNING "tvision fix: histlist.cpp not found among the tvision sources; not applied")
    else()
        list(REMOVE_AT _tv_sources ${_tv_index})
        list(APPEND _tv_sources "${_TV_FIXED}")
        set_property(TARGET tvision PROPERTY SOURCES ${_tv_sources})
        message(STATUS "Applied tvision fix: histlist.cpp (memmove)")
    endif()
else()
    message(STATUS "tvision fix for histlist.cpp not needed (source already changed)")
endif()
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${_TV_SRC}")
