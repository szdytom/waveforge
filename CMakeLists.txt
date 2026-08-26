cmake_minimum_required(VERSION 3.26)

project(waveforge VERSION 0.4 LANGUAGES C CXX)

option(WAVEFORGE_DEMO_VIDEO "Enable features for demo video recording" OFF)
option(WAVEFORGE_BUILD_BENCHMARKS "Build pixel physics benchmarks" OFF)
option(WAVEFORGE_ENABLE_WEBGPU "Build the authoritative WebGPU physics backend" ON)
option(WAVEFORGE_BUILD_TESTS "Build Waveforge unit tests" ON)

include(cmake/version.cmake)
include(cmake/deps.cmake)

set(WAVEFORGE_CPU_PHYSICS_SOURCES
    src/elements/air.cpp
    src/elements/copper.cpp
    src/elements/decoration.cpp
    src/elements/element.cpp
    src/elements/fluid.cpp
    src/elements/gas.cpp
    src/elements/oil.cpp
    src/elements/sand.cpp
    src/elements/smoke.cpp
    src/elements/steam.cpp
    src/elements/stone.cpp
    src/elements/water.cpp
    src/elements/wood.cpp
    src/fallsand/fluidflow.cpp
    src/fallsand/thermal.cpp
    src/fallsand/world.cpp
    src/2d.cpp
    src/xoroshiro.cpp
)

set(WAVEFORGE_GPU_PHYSICS_SOURCES
    src/fallsand/physics_gpu.cpp
    src/fallsand/physics_gpu_types.cpp
)

function(waveforge_configure_cpp_target target)
    target_compile_features(${target} PRIVATE cxx_std_23)
    target_include_directories(${target} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_BINARY_DIR}/include
    )
endfunction()

add_executable(waveforge
    ${WAVEFORGE_CPU_PHYSICS_SOURCES}
    src/fallsand/physics_gpu_types.cpp
    src/items/brush.cpp
    src/items/copper.cpp
    src/items/fire.cpp
    src/items/water.cpp
    src/items/oil.cpp
    src/scenes/duckdeath.cpp
    src/scenes/help.cpp
    src/scenes/level_menu.cpp
    src/scenes/level_switch.cpp
    src/scenes/level.cpp
    src/scenes/main_menu.cpp
    src/scenes/scene.cpp
    src/scenes/settings.cpp
    src/scenes/credits.cpp
    src/structures/electric.cpp
    src/structures/gate.cpp
    src/structures/heater.cpp
    src/structures/laser.cpp
    src/structures/position.cpp
    src/structures/power_source.cpp
    src/structures/pressure_plate.cpp
    src/structures/shape.cpp
    src/structures/tap.cpp
    src/structures/transistor.cpp
    src/animation.cpp
    src/assets.cpp
    src/audio.cpp
    src/checkpoint.cpp
    src/duck.cpp
    src/font.cpp
    src/level.cpp
    src/loader.cpp
    src/main.cpp
    src/save.cpp
)

waveforge_configure_cpp_target(waveforge)

target_link_libraries(waveforge PRIVATE
    SFML::Graphics
    SFML::Window
    SFML::System
    SFML::Audio
    msft_proxy4::proxy
    cpptrace::cpptrace
    nlohmann_json::nlohmann_json
    argparse::argparse
    qjs
)

if(WAVEFORGE_ENABLE_WEBGPU)
    target_sources(waveforge PRIVATE src/fallsand/physics_gpu.cpp)
    target_compile_definitions(waveforge PRIVATE WAVEFORGE_ENABLE_WEBGPU=1)
    waveforge_bundle_wgpu(waveforge)
endif()

if(WAVEFORGE_BUILD_BENCHMARKS)
    add_executable(pixel_physics_benchmark
        benchmarks/pixel_physics.cpp
        ${WAVEFORGE_CPU_PHYSICS_SOURCES}
    )

    waveforge_configure_cpp_target(pixel_physics_benchmark)
    target_link_libraries(pixel_physics_benchmark PRIVATE
        SFML::Graphics
        msft_proxy4::proxy
        cpptrace::cpptrace
    )
endif()

if(WAVEFORGE_ENABLE_WEBGPU AND WAVEFORGE_BUILD_BENCHMARKS)
    add_executable(webgpu_physics_benchmark
        benchmarks/webgpu_physics.cpp
        ${WAVEFORGE_GPU_PHYSICS_SOURCES}
    )
    waveforge_configure_cpp_target(webgpu_physics_benchmark)
    target_link_libraries(webgpu_physics_benchmark PRIVATE
        SFML::Graphics
        msft_proxy4::proxy
    )
    waveforge_bundle_wgpu(webgpu_physics_benchmark)
endif()

if(WAVEFORGE_BUILD_TESTS)
    enable_testing()
    add_executable(physics_gpu_types_test
        tests/physics_gpu_types.cpp
        src/fallsand/physics_gpu_types.cpp
    )
    waveforge_configure_cpp_target(physics_gpu_types_test)
    target_link_libraries(physics_gpu_types_test PRIVATE msft_proxy4::proxy)
    add_test(NAME physics_gpu_types COMMAND physics_gpu_types_test)
endif()
