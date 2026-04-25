include(FetchContent)

# SFML
FetchContent_Declare(SFML
    GIT_REPOSITORY https://github.com/SFML/SFML.git
    GIT_TAG 3.0.2
    GIT_SHALLOW ON
    EXCLUDE_FROM_ALL SYSTEM
)
option(SFML_USE_SYSTEM_DEPS ON)
FetchContent_MakeAvailable(SFML)

# msft_proxy
FetchContent_Declare(msft_proxy4
    GIT_REPOSITORY https://github.com/microsoft/proxy.git
    GIT_TAG 4.0.1
    GIT_SHALLOW ON
    EXCLUDE_FROM_ALL SYSTEM
)
FetchContent_MakeAvailable(msft_proxy4)

# cpptrace
FetchContent_Declare(
    cpptrace
    GIT_REPOSITORY https://github.com/jeremy-rifkin/cpptrace.git
    GIT_TAG        v1.0.4
    GIT_SHALLOW ON
    EXCLUDE_FROM_ALL SYSTEM
)
FetchContent_MakeAvailable(cpptrace)

# nlohmann_json
FetchContent_Declare(
    json
    URL https://github.com/nlohmann/json/releases/download/v3.12.0/json.tar.xz
    EXCLUDE_FROM_ALL SYSTEM
)
FetchContent_MakeAvailable(json)

# argparse
FetchContent_Declare(
    argparse
    GIT_REPOSITORY https://github.com/p-ranav/argparse.git
    GIT_TAG v3.2
    GIT_SHALLOW ON
    EXCLUDE_FROM_ALL SYSTEM
)
FetchContent_MakeAvailable(argparse)

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
)
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
