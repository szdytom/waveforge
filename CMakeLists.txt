cmake_minimum_required(VERSION 3.26)

project(waveforge VERSION 0.4 LANGUAGES C CXX)

option(WAVEFORGE_DEMO_VIDEO "Enable features for demo video recording" OFF)

include(cmake/version.cmake)
include(cmake/deps.cmake)

add_executable(waveforge
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
    src/2d.cpp
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
    src/xoroshiro.cpp
)

target_compile_features(waveforge PRIVATE cxx_std_23)

target_include_directories(waveforge PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_BINARY_DIR}/include
)

target_link_libraries(waveforge PRIVATE
    SFML::Graphics
    SFML::Window
    SFML::System
    SFML::Audio
    msft_proxy4::proxy
    cpptrace::cpptrace
    nlohmann_json::nlohmann_json
    argparse::argparse
    quickjs
)
