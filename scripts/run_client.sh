#!/bin/bash

# Run Feed Client

set -e

# Default parameters
HOST="127.0.0.1"
PORT=9876
NUM_SYMBOLS=100

# Parse arguments
if [ $# -ge 1 ]; then HOST=$1; fi
if [ $# -ge 2 ]; then PORT=$2; fi
if [ $# -ge 3 ]; then NUM_SYMBOLS=$3; fi

CLIENT_BIN="./build/release/feed_client"

# Check if binary exists
if [ ! -f "$CLIENT_BIN" ]; then
    echo "Client binary not found. Building..."
    ./scripts/build.sh
fi

echo "Starting Feed Handler Client..."
echo "  Server: $HOST:$PORT"
echo "  Symbols: $NUM_SYMBOLS"
echo ""

exec $CLIENT_BIN $HOST $PORT $NUM_SYMBOLS
