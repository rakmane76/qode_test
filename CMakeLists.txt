cmake_minimum_required(VERSION 3.10)
project(MarketDataFeedHandler VERSION 1.0)

# C++ Standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Compiler flags
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -march=native")
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG")
set(CMAKE_CXX_FLAGS_DEBUG "-g -O3 -DDEBUG")

# Include directories
include_directories(${PROJECT_SOURCE_DIR}/include)

# Find threads library
set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

# Common library
file(GLOB COMMON_SOURCES "src/common/*.cpp")
add_library(mdfh_common STATIC ${COMMON_SOURCES})
target_link_libraries(mdfh_common Threads::Threads)

# Server library
file(GLOB SERVER_SOURCES 
    "src/server/exchange_simulator.cpp"
    "src/server/tick_generator.cpp"
    "src/server/client_manager.cpp"
)
add_library(mdfh_server STATIC ${SERVER_SOURCES})
target_link_libraries(mdfh_server mdfh_common Threads::Threads)

# Client library
file(GLOB CLIENT_SOURCES 
    "src/client/socket.cpp"
    "src/client/parser.cpp"
    "src/client/feed_handler.cpp"
    "src/client/visualizer.cpp"
)
add_library(mdfh_client STATIC ${CLIENT_SOURCES})
target_link_libraries(mdfh_client mdfh_common Threads::Threads)

# Server executable
add_executable(exchange_server src/server/server_main.cpp)
target_link_libraries(exchange_server mdfh_server mdfh_common Threads::Threads)

# Client executable
add_executable(feed_client src/client/client_main.cpp)
target_link_libraries(feed_client mdfh_client mdfh_common Threads::Threads)

# Install targets
install(TARGETS exchange_server feed_client
        RUNTIME DESTINATION bin)

# Testing (Google Test)
option(BUILD_TESTING "Build tests" ON)
if(BUILD_TESTING)
    enable_testing()
    find_package(GTest REQUIRED)
    include_directories(${GTEST_INCLUDE_DIRS})
    
    # Add TESTING definition to libraries when building tests
    target_compile_definitions(mdfh_server PUBLIC TESTING)
    target_compile_definitions(mdfh_common PUBLIC TESTING)
    target_compile_definitions(mdfh_client PUBLIC TESTING)
    
    # Set output directory for test binaries
    set(TEST_OUTPUT_DIR ${CMAKE_BINARY_DIR}/tests)
    
    # Unit tests
    add_executable(protocol_test tests/unit/test_protocol.cpp)
    target_compile_definitions(protocol_test PRIVATE TESTING)
    target_link_libraries(protocol_test mdfh_common ${GTEST_LIBRARIES} pthread)
    set_target_properties(protocol_test PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${TEST_OUTPUT_DIR})
    add_test(NAME protocol_test COMMAND protocol_test)
    
    add_executable(parser_test tests/unit/test_parser.cpp)
    target_compile_definitions(parser_test PRIVATE TESTING)
    target_link_libraries(parser_test mdfh_client mdfh_common ${GTEST_LIBRARIES} pthread)
    set_target_properties(parser_test PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${TEST_OUTPUT_DIR})
    add_test(NAME parser_test COMMAND parser_test)
    
    add_executable(cache_test tests/unit/test_cache.cpp)
    target_compile_definitions(cache_test PRIVATE TESTING)
    target_link_libraries(cache_test mdfh_common ${GTEST_LIBRARIES} pthread)
    set_target_properties(cache_test PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${TEST_OUTPUT_DIR})
    add_test(NAME cache_test COMMAND cache_test)
    
    add_executable(latency_tracker_test tests/unit/test_latency_tracker.cpp)
    target_compile_definitions(latency_tracker_test PRIVATE TESTING)
    target_link_libraries(latency_tracker_test mdfh_common ${GTEST_LIBRARIES} pthread)
    set_target_properties(latency_tracker_test PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${TEST_OUTPUT_DIR})
    add_test(NAME latency_tracker_test COMMAND latency_tracker_test)
    
    add_executable(memory_pool_test tests/unit/test_memory_pool.cpp)
    target_compile_definitions(memory_pool_test PRIVATE TESTING)
    target_link_libraries(memory_pool_test mdfh_common ${GTEST_LIBRARIES} pthread)
    set_target_properties(memory_pool_test PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${TEST_OUTPUT_DIR})
    add_test(NAME memory_pool_test COMMAND memory_pool_test)
    
    add_executable(tick_generator_test tests/unit/test_tick_generator.cpp)
    target_compile_definitions(tick_generator_test PRIVATE TESTING)
    target_link_libraries(tick_generator_test mdfh_server ${GTEST_LIBRARIES} pthread)
    set_target_properties(tick_generator_test PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${TEST_OUTPUT_DIR})
    add_test(NAME tick_generator_test COMMAND tick_generator_test)
    
    add_executable(exchange_simulator_test tests/unit/test_exchange_simulator.cpp)
    target_compile_definitions(exchange_simulator_test PRIVATE TESTING)
    target_link_libraries(exchange_simulator_test mdfh_server mdfh_common ${GTEST_LIBRARIES} pthread)
    set_target_properties(exchange_simulator_test PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${TEST_OUTPUT_DIR})
    add_test(NAME exchange_simulator_test COMMAND exchange_simulator_test)
    
    add_executable(subscription_test tests/unit/test_subscription.cpp)
    target_compile_definitions(subscription_test PRIVATE TESTING)
    target_link_libraries(subscription_test mdfh_server mdfh_common ${GTEST_LIBRARIES} pthread)
    set_target_properties(subscription_test PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${TEST_OUTPUT_DIR})
    add_test(NAME subscription_test COMMAND subscription_test)
    
    add_executable(config_parser_test tests/unit/test_config_parser.cpp)
    target_compile_definitions(config_parser_test PRIVATE TESTING)
    target_link_libraries(config_parser_test mdfh_common ${GTEST_LIBRARIES} pthread)
    set_target_properties(config_parser_test PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${TEST_OUTPUT_DIR})
    add_test(NAME config_parser_test COMMAND config_parser_test)
    
    add_executable(visualizer_test tests/unit/test_visualizer.cpp)
    target_compile_definitions(visualizer_test PRIVATE TESTING)
    target_link_libraries(visualizer_test mdfh_client mdfh_common ${GTEST_LIBRARIES} pthread)
    set_target_properties(visualizer_test PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${TEST_OUTPUT_DIR})
    add_test(NAME visualizer_test COMMAND visualizer_test)
    
    add_executable(socket_test tests/unit/test_socket.cpp)
    target_compile_definitions(socket_test PRIVATE TESTING)
    target_link_libraries(socket_test mdfh_client mdfh_common ${GTEST_LIBRARIES} pthread)
    set_target_properties(socket_test PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${TEST_OUTPUT_DIR})
    add_test(NAME socket_test COMMAND socket_test)
    
    add_executable(feed_handler_test tests/unit/test_feed_handler.cpp)
    target_compile_definitions(feed_handler_test PRIVATE TESTING)
    target_link_libraries(feed_handler_test mdfh_client mdfh_common ${GTEST_LIBRARIES} pthread)
    set_target_properties(feed_handler_test PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${TEST_OUTPUT_DIR})
    add_test(NAME feed_handler_test COMMAND feed_handler_test)
    
    add_executable(client_manager_test tests/unit/test_client_manager.cpp)
    target_compile_definitions(client_manager_test PRIVATE TESTING)
    target_link_libraries(client_manager_test mdfh_server mdfh_common ${GTEST_LIBRARIES} pthread)
    set_target_properties(client_manager_test PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${TEST_OUTPUT_DIR})
    add_test(NAME client_manager_test COMMAND client_manager_test)
    
endif()

# Benchmarks (Google Benchmark)
option(BUILD_BENCHMARKS "Build benchmarks" OFF)
if(BUILD_BENCHMARKS)
    find_package(benchmark QUIET)
    
    if(benchmark_FOUND)
        message(STATUS "Google Benchmark found - building benchmarks")
        
        # Set output directory for benchmark binaries
        set(BENCHMARK_OUTPUT_DIR ${CMAKE_BINARY_DIR}/benchmark)
        
        # Parser benchmark
        add_executable(parser_benchmark benchmarks/parser_benchmark.cpp src/client/parser.cpp)
        target_link_libraries(parser_benchmark benchmark::benchmark benchmark::benchmark_main pthread)
        set_target_properties(parser_benchmark PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${BENCHMARK_OUTPUT_DIR})
        
        # Cache benchmark
        add_executable(cache_benchmark benchmarks/cache_benchmark.cpp src/common/cache.cpp)
        target_link_libraries(cache_benchmark benchmark::benchmark benchmark::benchmark_main pthread)
        set_target_properties(cache_benchmark PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${BENCHMARK_OUTPUT_DIR})
        
        # Latency benchmark
        add_executable(latency_benchmark benchmarks/latency_benchmark.cpp src/common/latency_tracker.cpp)
        target_link_libraries(latency_benchmark benchmark::benchmark benchmark::benchmark_main pthread)
        set_target_properties(latency_benchmark PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${BENCHMARK_OUTPUT_DIR})
        
        # Tick generator benchmark
        add_executable(tick_generator_benchmark benchmarks/tick_generator_benchmark.cpp src/server/tick_generator.cpp)
        target_link_libraries(tick_generator_benchmark benchmark::benchmark benchmark::benchmark_main pthread)
        set_target_properties(tick_generator_benchmark PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${BENCHMARK_OUTPUT_DIR})
        
        # Memory pool benchmark
        add_executable(memory_pool_benchmark benchmarks/memory_pool_benchmark.cpp src/common/memory_pool.cpp)
        target_link_libraries(memory_pool_benchmark benchmark::benchmark benchmark::benchmark_main pthread)
        set_target_properties(memory_pool_benchmark PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${BENCHMARK_OUTPUT_DIR})
        
        # Socket benchmark
        add_executable(socket_benchmark benchmarks/socket_benchmark.cpp)
        target_link_libraries(socket_benchmark mdfh_client benchmark::benchmark benchmark::benchmark_main pthread)
        set_target_properties(socket_benchmark PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${BENCHMARK_OUTPUT_DIR})
    else()
        message(WARNING "Google Benchmark not found. Skipping benchmark builds.")
        message(WARNING "Install with: sudo apt-get install libbenchmark-dev")
        message(WARNING "Or build from source: https://github.com/google/benchmark")
    endif()
endif()

# Print build configuration
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "C++ compiler: ${CMAKE_CXX_COMPILER}")
message(STATUS "C++ flags: ${CMAKE_CXX_FLAGS}")
message(STATUS "Build testing: ${BUILD_TESTING}")
message(STATUS "Build benchmarks: ${BUILD_BENCHMARKS}")
