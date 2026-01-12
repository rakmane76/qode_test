cmake_minimum_required(VERSION 3.14)

# GoogleTest
find_package(GTest REQUIRED)
include_directories(${GTEST_INCLUDE_DIRS})

# Set output directory for test binaries
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/tests)

# Unit test executables
add_executable(test_tick_generator test_tick_generator.cpp)
target_compile_definitions(test_tick_generator PRIVATE TESTING)
target_link_libraries(test_tick_generator 
    server_lib 
    common_lib 
    GTest::gtest 
    GTest::gtest_main
    pthread
)

add_executable(test_parser test_parser.cpp)
target_compile_definitions(test_parser PRIVATE TESTING)
target_link_libraries(test_parser 
    client_lib 
    common_lib 
    GTest::gtest 
    GTest::gtest_main
    pthread
)

add_executable(test_cache test_cache.cpp)
target_compile_definitions(test_cache PRIVATE TESTING)
target_link_libraries(test_cache 
    common_lib 
    GTest::gtest 
    GTest::gtest_main
    pthread
)

add_executable(test_latency_tracker test_latency_tracker.cpp)
target_compile_definitions(test_latency_tracker PRIVATE TESTING)
target_link_libraries(test_latency_tracker 
    common_lib 
    GTest::gtest 
    GTest::gtest_main
    pthread
)

add_executable(test_memory_pool test_memory_pool.cpp)
target_compile_definitions(test_memory_pool PRIVATE TESTING)
target_link_libraries(test_memory_pool 
    common_lib 
    GTest::gtest 
    GTest::gtest_main
    pthread
)

add_executable(exchange_simulator_test test_exchange_simulator.cpp)
target_compile_definitions(exchange_simulator_test PRIVATE TESTING)
target_link_libraries(exchange_simulator_test 
    server_lib 
    common_lib 
    GTest::gtest 
    GTest::gtest_main
    pthread
)

add_executable(config_parser_test test_config_parser.cpp)
target_compile_definitions(config_parser_test PRIVATE TESTING)
target_link_libraries(config_parser_test 
    common_lib 
    GTest::gtest 
    GTest::gtest_main
    pthread
)

# Add tests to CTest
add_test(NAME TickGeneratorTest COMMAND test_tick_generator)
add_test(NAME ParserTest COMMAND test_parser)
add_test(NAME CacheTest COMMAND test_cache)
add_test(NAME LatencyTrackerTest COMMAND test_latency_tracker)
add_test(NAME MemoryPoolTest COMMAND test_memory_pool)
add_test(NAME ExchangeSimulatorTest COMMAND exchange_simulator_test)
add_test(NAME ConfigParserTest COMMAND config_parser_test)
