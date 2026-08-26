set(WGPU_NATIVE_VERSION 27.0.4.0)
set(WGPU_NATIVE_BASE_URL
    "https://github.com/gfx-rs/wgpu-native/releases/download/v${WGPU_NATIVE_VERSION}"
)

if(WIN32 AND CMAKE_SIZEOF_VOID_P EQUAL 8)
    set(WGPU_NATIVE_ARCHIVE "wgpu-windows-x86_64-msvc-release.zip")
    set(WGPU_NATIVE_HASH "f14ca334b4d253881bde2605bd147f332178d705f56fbd74f81458797c77fce1")
elseif(APPLE AND CMAKE_SYSTEM_PROCESSOR MATCHES "^(arm64|aarch64)$")
    set(WGPU_NATIVE_ARCHIVE "wgpu-macos-aarch64-release.zip")
    set(WGPU_NATIVE_HASH "15367c26fdbe6892db35007d39f3883593384e777360b70e6bd704cb5dedde53")
elseif(APPLE AND CMAKE_SIZEOF_VOID_P EQUAL 8)
    set(WGPU_NATIVE_ARCHIVE "wgpu-macos-x86_64-release.zip")
    set(WGPU_NATIVE_HASH "660fe9be59b555ec1d7c839e5cf8b6c71762938af61ab444a7a58dd87970dba2")
elseif(UNIX AND NOT APPLE AND CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|amd64|AMD64)$")
    set(WGPU_NATIVE_ARCHIVE "wgpu-linux-x86_64-release.zip")
    set(WGPU_NATIVE_HASH "271481ef76fbf3ea09631a6079e9493636ecf813cd9c92306c44a1a452991ba1")
else()
    message(FATAL_ERROR "No pinned wgpu-native binary for this platform")
endif()

FetchContent_Declare(wgpu_native_distribution
    URL "${WGPU_NATIVE_BASE_URL}/${WGPU_NATIVE_ARCHIVE}"
    URL_HASH "SHA256=${WGPU_NATIVE_HASH}"
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_MakeAvailable(wgpu_native_distribution)

add_library(wgpu_native SHARED IMPORTED GLOBAL)
target_include_directories(wgpu_native SYSTEM INTERFACE
    "${wgpu_native_distribution_SOURCE_DIR}/include"
)

if(WIN32)
    set_target_properties(wgpu_native PROPERTIES
        IMPORTED_IMPLIB "${wgpu_native_distribution_SOURCE_DIR}/lib/wgpu_native.lib"
        IMPORTED_LOCATION "${wgpu_native_distribution_SOURCE_DIR}/lib/wgpu_native.dll"
    )
elseif(APPLE)
    set_target_properties(wgpu_native PROPERTIES
        IMPORTED_LOCATION "${wgpu_native_distribution_SOURCE_DIR}/lib/libwgpu_native.dylib"
    )
else()
    set_target_properties(wgpu_native PROPERTIES
        IMPORTED_LOCATION "${wgpu_native_distribution_SOURCE_DIR}/lib/libwgpu_native.so"
        IMPORTED_NO_SONAME TRUE
    )
endif()

function(waveforge_bundle_wgpu target)
    target_link_libraries(${target} PRIVATE wgpu_native)

    if(APPLE)
        set_target_properties(${target} PROPERTIES BUILD_RPATH "@loader_path")
    elseif(UNIX)
        set_target_properties(${target} PROPERTIES BUILD_RPATH "$ORIGIN")
    endif()

    add_custom_command(TARGET ${target} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:wgpu_native>
            $<TARGET_FILE_DIR:${target}>
    )
endfunction()
