cmake_minimum_required(VERSION 3.26)

include(FetchContent)

find_package(Git QUIET)
if(GIT_FOUND)
    execute_process(
        COMMAND ${GIT_EXECUTABLE} rev-parse --short HEAD
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        OUTPUT_VARIABLE WAVEFORGE_GIT_COMMIT_HASH
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
    )
else()
	set(WAVEFORGE_GIT_COMMIT_HASH "")
endif()

set(WAVEFORGE_VERSION "0.4")

project(waveforge VERSION ${WAVEFORGE_VERSION} LANGUAGES CXX)

option(WAVEFORGE_DEMO_VIDEO "Enable features for demo video recording" OFF)

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

target_compile_definitions(waveforge PRIVATE
	"WAVEFORGE_VERSION=\"${WAVEFORGE_VERSION}\""
)

if (WAVEFORGE_GIT_COMMIT_HASH)
	target_compile_definitions(waveforge PRIVATE
		"WAVEFORGE_GIT_COMMIT_HASH=\"${WAVEFORGE_GIT_COMMIT_HASH}\""
	)
endif()

target_include_directories(waveforge PRIVATE
	${CMAKE_CURRENT_SOURCE_DIR}/include
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
)

if (WAVEFORGE_DEMO_VIDEO)
	target_compile_definitions(waveforge PRIVATE
		"WAVEFORGE_DEMO_VIDEO=1"
	)
endif()
