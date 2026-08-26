include(FetchContent)

if(WAVEFORGE_ENABLE_WEBGPU)
    include("${CMAKE_CURRENT_LIST_DIR}/wgpu.cmake")
endif()

# For some reasons, msft_proxy4's CMakeLists.txt is not compatible with SFML's CMakeLists.txt
# Therefore, msft_proxy4 must be made available before SFML.

# msft_proxy
FetchContent_Declare(msft_proxy4
    GIT_REPOSITORY https://github.com/ngcpp/proxy.git
    GIT_TAG 4.0.2
    GIT_SHALLOW ON
    EXCLUDE_FROM_ALL SYSTEM
)
FetchContent_MakeAvailable(msft_proxy4)

# SFML
FetchContent_Declare(SFML
    GIT_REPOSITORY https://github.com/SFML/SFML.git
    GIT_TAG 3.1.0
    GIT_SHALLOW ON
    EXCLUDE_FROM_ALL SYSTEM
)
option(SFML_USE_SYSTEM_DEPS ON)
FetchContent_MakeAvailable(SFML)

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

# QuickJS: Next Generation
# IMPORTANT NOTE ON FUTURE UPDATES: The "js_hacks.h" header heavily relies on the internal
# structure of QuickJS, so it MUST BE REVIEWED after updating the QuickJS submodule, even for
# minor updates.
FetchContent_Declare(
	quickjs
	GIT_REPOSITORY https://github.com/quickjs-ng/quickjs.git
	GIT_TAG v0.14.0
	GIT_SHALLOW ON
	EXCLUDE_FROM_ALL SYSTEM
)
FetchContent_MakeAvailable(quickjs)
