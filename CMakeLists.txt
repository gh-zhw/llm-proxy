cmake_minimum_required(VERSION 3.22)
project(llm-proxy VERSION 0.1.0 LANGUAGES CXX)

# Enable testing (for later unit tests)
include(CTest)

# Set C++ standard to 20
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Set output directory for executables
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

include(FetchContent)

# Declare dependencies (using stable versions)
# 1. cpp-httplib
FetchContent_Declare(
    httplib
    GIT_REPOSITORY https://github.com/yhirose/cpp-httplib.git
    GIT_TAG v0.18.0
)

# 2. spdlog
FetchContent_Declare(
    spdlog
    GIT_REPOSITORY https://github.com/gabime/spdlog.git
    GIT_TAG v1.14.1
)

# 3. yaml-cpp
FetchContent_Declare(
    yaml-cpp
    GIT_REPOSITORY https://github.com/jbeder/yaml-cpp.git
    GIT_TAG 0.8.0
)

# 4. nlohmann/json
FetchContent_Declare(
    nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.11.3
)

# 5. googletest (disable install options)
FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.14.0
    CMAKE_ARGS -DINSTALL_GTEST=OFF
)

# Fetch and build all dependencies
FetchContent_MakeAvailable(
    httplib
    spdlog
    yaml-cpp
    nlohmann_json
    googletest
)

# Add the main executable
add_executable(llm-proxy
    src/main.cpp
    src/utils/logger.cpp
    src/config/config_manager.cpp
    src/server/http_server.cpp
)

# Add include directory
target_include_directories(llm-proxy PRIVATE include)

# Link all required libraries
target_link_libraries(llm-proxy
    PRIVATE
        httplib::httplib
        spdlog::spdlog
        yaml-cpp
        nlohmann_json::nlohmann_json
        GTest::gtest
)

message(STATUS "C++ Standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "Binary output directory: ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
