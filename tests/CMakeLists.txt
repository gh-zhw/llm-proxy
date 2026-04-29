# Unit tests for llm-proxy

# Common include directory
set(INCLUDE_DIR ${CMAKE_SOURCE_DIR}/include)

# Cache test: needs cache.cpp, logger.cpp, spdlog
add_executable(cache_test
    cache_test.cpp
    ${CMAKE_SOURCE_DIR}/src/cache/cache.cpp
    ${CMAKE_SOURCE_DIR}/src/utils/logger.cpp
)
target_link_libraries(cache_test PRIVATE GTest::gtest_main spdlog::spdlog)
target_include_directories(cache_test PRIVATE ${INCLUDE_DIR})

# Config test: needs config_manager.cpp, logger.cpp, spdlog, yaml-cpp
add_executable(config_test
    config_test.cpp
    ${CMAKE_SOURCE_DIR}/src/config/config_manager.cpp
    ${CMAKE_SOURCE_DIR}/src/utils/logger.cpp
)
target_link_libraries(config_test PRIVATE GTest::gtest_main spdlog::spdlog yaml-cpp)
target_include_directories(config_test PRIVATE ${INCLUDE_DIR})

# Parser test: needs request_parser.cpp, logger.cpp, spdlog, nlohmann_json
add_executable(parser_test
    parser_test.cpp
    ${CMAKE_SOURCE_DIR}/src/parser/request_parser.cpp
    ${CMAKE_SOURCE_DIR}/src/utils/logger.cpp
)
target_link_libraries(parser_test PRIVATE GTest::gtest_main spdlog::spdlog nlohmann_json::nlohmann_json)
target_include_directories(parser_test PRIVATE ${INCLUDE_DIR})

# Metrics test: only metrics.cpp
add_executable(metrics_test
    metrics_test.cpp
    ${CMAKE_SOURCE_DIR}/src/metrics/metrics.cpp
)
target_link_libraries(metrics_test PRIVATE GTest::gtest_main)
target_include_directories(metrics_test PRIVATE ${INCLUDE_DIR})

# Register tests with ctest
add_test(NAME CacheTest COMMAND cache_test)
add_test(NAME ConfigTest COMMAND config_test)
add_test(NAME ParserTest COMMAND parser_test)
add_test(NAME MetricsTest COMMAND metrics_test)
