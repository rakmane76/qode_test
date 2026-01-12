#!/bin/bash

# Run Complete Demo

set -e

echo "=== Market Data Feed Handler Demo ==="
echo ""

# Build if needed
if [ ! -f "build/release/exchange_server" ] || [ ! -f "build/release/feed_client" ]; then
    echo "Building project..."
    ./scripts/build.sh
    echo ""
fi

mkdir -p ./logs

# Start server in background
echo "Starting server on port 9876..."
./scripts/run_server.sh > logs/server.log 2>&1 &
SERVER_PID=$!

echo "Server started (PID: $SERVER_PID)"
echo ""

# Wait for server to start
sleep 2

# Start client
echo "Starting client..."
echo ""
./scripts/run_client.sh 

# Cleanup on exit
kill $SERVER_PID 2>/dev/null || true

echo ""
echo "Demo complete"
