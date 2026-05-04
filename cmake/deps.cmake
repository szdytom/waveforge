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

include(${CMAKE_CURRENT_LIST_DIR}/deps-quickjs.cmake)
