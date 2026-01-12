#!/bin/bash

# Script to run visualizer unit tests

set -e

SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
PROJECT_ROOT="$SCRIPT_DIR/.."

cd "$PROJECT_ROOT"

echo "=== Visualizer Unit Tests ==="
echo ""

# Build if needed
if [ ! -f "build/tests/visualizer_test" ]; then
    echo "Building visualizer tests..."
    cmake --build build --target visualizer_test
    echo ""
fi

# Run the test
echo "Running visualizer tests..."
echo ""

./build/tests/visualizer_test

echo ""
echo "All visualizer tests completed!"
