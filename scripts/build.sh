#!/usr/bin/bash

# Market Data Feed Handler - Build Script

set -e  # Exit on error

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

echo -e "${GREEN}=== Market Data Feed Handler - Build Script ===${NC}"
echo ""

# Parse arguments
BUILD_TYPE="Release"
CLEAN=false
VERBOSE=false
BUILD_TESTS=ON
BUILD_BENCHMARKS=OFF

while [[ $# -gt 0 ]]; do
    case $1 in
        -d|--debug)
            BUILD_TYPE="Debug"
            shift
            ;;
        -c|--clean)
            CLEAN=true
            shift
            ;;
        -v|--verbose)
            VERBOSE=true
            shift
            ;;
        -t|--tests)
            BUILD_TESTS=ON
            shift
            ;;
        -b|--benchmarks)
            BUILD_BENCHMARKS=ON
            shift
            ;;
        -h|--help)
            echo "Usage: $0 [OPTIONS]"
            echo ""
            echo "Options:"
            echo "  -d, --debug       Build debug version with symbols"
            echo "  -c, --clean       Clean before building"
            echo "  -v, --verbose     Verbose build output"
            echo "  -t, --tests       Build tests (default: ON)"
            echo "  -b, --benchmarks  Build benchmarks (default: OFF)"
            echo "  -h, --help        Show this help message"
            echo ""
            exit 0
            ;;
        *)
            echo -e "${RED}Unknown option: $1${NC}"
            exit 1
            ;;
    esac
done

# Create build directory based on build type
if [ "$BUILD_TYPE" = "Debug" ]; then
    BUILD_DIR="build/debug"
else
    BUILD_DIR="build/release"
fi

if [ ! -d "$BUILD_DIR" ]; then
    echo -e "${BLUE}Creating build directory: ${BUILD_DIR}...${NC}"
    mkdir -p "$BUILD_DIR"
fi

# Clean if requested
if [ "$CLEAN" = true ]; then
    echo -e "${YELLOW}Cleaning build artifacts...${NC}"
    rm -rf "$BUILD_DIR"/*
    mkdir -p "$BUILD_DIR"
    echo ""
fi

cd "$BUILD_DIR"

# Configure with CMake
echo -e "${BLUE}Configuring with CMake...${NC}"
echo -e "  Build Type: ${BUILD_TYPE}"
echo -e "  Tests: ${BUILD_TESTS}"
echo -e "  Benchmarks: ${BUILD_BENCHMARKS}"
echo ""

CMAKE_ARGS="-DCMAKE_BUILD_TYPE=${BUILD_TYPE} -DBUILD_TESTING=${BUILD_TESTS} -DBUILD_BENCHMARKS=${BUILD_BENCHMARKS}"

# Determine correct path to project root
if [ "$BUILD_TYPE" = "Debug" ]; then
    PROJECT_ROOT="../.."
else
    PROJECT_ROOT="../.."
fi

if [ "$VERBOSE" = true ]; then
    cmake $PROJECT_ROOT $CMAKE_ARGS -DCMAKE_VERBOSE_MAKEFILE=ON
else
    cmake $PROJECT_ROOT $CMAKE_ARGS
fi

if [ $? -ne 0 ]; then
    echo -e "${RED}CMake configuration failed!${NC}"
    exit 1
fi

# Build
echo ""
echo -e "${BLUE}Building ${BUILD_TYPE} version...${NC}"
echo ""

if [ "$VERBOSE" = true ]; then
    make -j$(nproc) VERBOSE=1
else
    make -j$(nproc)
fi

if [ $? -ne 0 ]; then
    echo -e "${RED}Build failed!${NC}"
    exit 1
fi

echo ""
echo -e "${GREEN}=== Build Complete ===${NC}"
echo ""
echo -e "Executables:"
echo -e "  Server: ${GREEN}${BUILD_DIR}/exchange_server${NC}"
echo -e "  Client: ${GREEN}${BUILD_DIR}/feed_client${NC}"

if [ "$BUILD_TESTS" = "ON" ]; then
    echo ""
    echo -e "Tests:"
    echo -e "  ${GREEN}${BUILD_DIR}/tests/protocol_test${NC}"
    echo -e "  ${GREEN}${BUILD_DIR}/tests/parser_test${NC}"
    echo -e "  ${GREEN}${BUILD_DIR}/tests/cache_test${NC}"
    echo -e "  ${GREEN}${BUILD_DIR}/tests/latency_tracker_test${NC}"
    echo -e "  ${GREEN}${BUILD_DIR}/tests/memory_pool_test${NC}"
    echo -e "  ${GREEN}${BUILD_DIR}/tests/tick_generator_test${NC}"
    echo -e "  ${GREEN}${BUILD_DIR}/tests/exchange_simulator_test${NC}"
    echo -e "  ${GREEN}${BUILD_DIR}/tests/subscription_test${NC}"
    echo -e "  ${GREEN}${BUILD_DIR}/tests/config_parser_test${NC}"
    echo -e "  ${GREEN}${BUILD_DIR}/tests/visualizer_test${NC}"
    echo -e "  ${GREEN}${BUILD_DIR}/tests/socket_test${NC}"
    echo -e "  ${GREEN}${BUILD_DIR}/tests/feed_handler_test${NC}"
    echo -e "  ${GREEN}${BUILD_DIR}/tests/client_manager_test${NC}"
fi

if [ "$BUILD_BENCHMARKS" = "ON" ]; then
    echo ""
    echo -e "Benchmarks:"
    echo -e "  ${GREEN}${BUILD_DIR}/benchmark/parser_benchmark${NC}"
    echo -e "  ${GREEN}${BUILD_DIR}/benchmark/cache_benchmark${NC}"
    echo -e "  ${GREEN}${BUILD_DIR}/benchmark/latency_benchmark${NC}"
    echo -e "  ${GREEN}${BUILD_DIR}/benchmark/tick_generator_benchmark${NC}"
    echo -e "  ${GREEN}${BUILD_DIR}/benchmark/memory_pool_benchmark${NC}"
    echo -e "  ${GREEN}${BUILD_DIR}/benchmark/socket_benchmark${NC}"
fi

echo ""
echo -e "Run with:"
echo -e "  ${BLUE}./scripts/run_server.sh${NC}"
echo -e "  ${BLUE}./scripts/run_client.sh${NC}"
echo -e "  ${BLUE}./scripts/run_demo.sh${NC}"

if [ "$BUILD_TESTS" = "ON" ]; then
    echo ""
    echo -e "Run tests:"
    echo -e "  ${BLUE}cd ${BUILD_DIR} && ctest${NC}"
fi

if [ "$BUILD_BENCHMARKS" = "ON" ]; then
    echo ""
    echo -e "Run benchmarks:"
    echo -e "  ${BLUE}./scripts/run_benchmarks.sh${NC}"
fi

echo ""

