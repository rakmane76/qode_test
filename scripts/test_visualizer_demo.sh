#!/bin/bash

# Market Data Feed Handler - Visualizer Demo Script
# This script demonstrates the visualizer in action with a live server

set -e

SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
PROJECT_ROOT="$SCRIPT_DIR/.."

cd "$PROJECT_ROOT"

echo "=== Market Data Feed Handler - Visualizer Demo ==="
echo ""

# Check if binaries exist
if [ ! -f "build/exchange_server" ] || [ ! -f "build/feed_client" ]; then
    echo "Building project..."
    ./scripts/build.sh
fi

# Cleanup function
cleanup() {
    echo ""
    echo "Shutting down..."
    if [ ! -z "$SERVER_PID" ]; then
        kill $SERVER_PID 2>/dev/null || true
        wait $SERVER_PID 2>/dev/null || true
    fi
    if [ ! -z "$CLIENT_PID" ]; then
        kill $CLIENT_PID 2>/dev/null || true
        wait $CLIENT_PID 2>/dev/null || true
    fi
    echo "Demo stopped."
}

trap cleanup EXIT INT TERM

# Start the exchange simulator server
echo "Starting Exchange Simulator Server..."
./build/exchange_server > /tmp/mdfh_server.log 2>&1 &
SERVER_PID=$!

# Wait for server to start
sleep 2

# Check if server is running
if ! kill -0 $SERVER_PID 2>/dev/null; then
    echo "ERROR: Server failed to start. Check /tmp/mdfh_server.log"
    cat /tmp/mdfh_server.log
    exit 1
fi

echo "Server started (PID: $SERVER_PID)"
echo ""
echo "========================================"
echo "Starting Feed Client with Visualizer..."
echo "========================================"
echo ""
echo "Instructions:"
echo "  - The visualizer shows top 20 most active symbols"
echo "  - Updates every 500ms with real-time data"
echo "  - Press Ctrl+C to stop"
echo ""
sleep 2

# Start the feed client with visualizer
# Subscribe to all symbols (0-19)
SYMBOLS="0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19"
./build/feed_client &
CLIENT_PID=$!

# Wait for client to run
wait $CLIENT_PID

echo ""
echo "Demo completed!"
