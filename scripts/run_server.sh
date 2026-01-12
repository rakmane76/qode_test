#!/bin/bash

# Run Exchange Server

set -e

SERVER_BIN="./build/release/exchange_server"

# Check if binary exists
if [ ! -f "$SERVER_BIN" ]; then
    echo "Server binary not found. Building..."
    ./scripts/build.sh
fi

echo "Starting Exchange Server..."
echo ""

exec $SERVER_BIN 
