# QuickJS (no CMakeLists.txt — download and build manually)
set(QUICKJS_URL https://bellard.org/quickjs/quickjs-2025-09-13-2.tar.xz)
set(QUICKJS_ARCHIVE ${CMAKE_BINARY_DIR}/_deps/quickjs.tar.xz)
set(QUICKJS_SOURCE_DIR ${CMAKE_BINARY_DIR}/_deps/quickjs-src)

if(NOT EXISTS ${QUICKJS_SOURCE_DIR}/quickjs.c)
    file(DOWNLOAD ${QUICKJS_URL} ${QUICKJS_ARCHIVE}
        EXPECTED_HASH SHA256=996c6b5018fc955ad4d06426d0e9cb713685a00c825aa5c0418bd53f7df8b0b4
        SHOW_PROGRESS
    )
    file(ARCHIVE_EXTRACT INPUT ${QUICKJS_ARCHIVE}
        DESTINATION ${CMAKE_BINARY_DIR}/_deps
    )
    file(RENAME ${CMAKE_BINARY_DIR}/_deps/quickjs-2025-09-13 ${QUICKJS_SOURCE_DIR})
    file(REMOVE ${QUICKJS_ARCHIVE})
endif()

add_library(quickjs STATIC
    ${QUICKJS_SOURCE_DIR}/quickjs.c
    ${QUICKJS_SOURCE_DIR}/libregexp.c
    ${QUICKJS_SOURCE_DIR}/libunicode.c
    ${QUICKJS_SOURCE_DIR}/cutils.c
    ${QUICKJS_SOURCE_DIR}/dtoa.c
    ${QUICKJS_SOURCE_DIR}/quickjs-libc.c
)
target_link_libraries(quickjs PUBLIC m)
target_include_directories(quickjs PUBLIC ${QUICKJS_SOURCE_DIR})
target_compile_definitions(quickjs PRIVATE
    _GNU_SOURCE
    CONFIG_VERSION=\"2025-09-13-2\"
)
target_compile_options(quickjs PRIVATE -Wall -Wno-unused-parameter -Wno-sign-compare)
set_target_properties(quickjs PROPERTIES
    C_STANDARD 11
    C_STANDARD_REQUIRED ON
    POSITION_INDEPENDENT_CODE ON
)
