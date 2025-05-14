#!/bin/bash
# Script to build Snapcast on Linux with chrony support

# Make sure we're in the right directory
cd "$(dirname "$0")"

# Create build directory if it doesn't exist
mkdir -p build
cd build

# Configure with cmake
cmake ..

# Build with multiple cores
make -j$(nproc)

echo "Build complete. Check for any errors above."
