#!/bin/bash
# build-snapcast.sh - Optimized build script for Snapcast

# Configure ccache for maximum performance
export CCACHE_COMPRESS=1
export CCACHE_MAXSIZE=5G
export CCACHE_SLOPPINESS=pch_defines,time_macros

# Set Ninja status for better progress reporting
export NINJA_STATUS="[%p/%f/%t] "

# Completely remove and recreate build directory
echo "Cleaning build directory..."
rm -rf build
mkdir -p build
cd build

# Make sure there's no CMakeCache.txt
if [ -f "CMakeCache.txt" ]; then
  rm -f CMakeCache.txt
fi

# Configure with CMake and optimizations
echo "Configuring with CMake..."
cmake -GNinja \
  -DCMAKE_CXX_COMPILER_LAUNCHER=ccache \
  -DCMAKE_C_COMPILER_LAUNCHER=ccache \
  -DBUILD_CLIENT=ON \
  -DBUILD_SERVER=ON \
  -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_WITH_PULSE=OFF \
  -DCMAKE_CXX_FLAGS="-pipe -O2" \
  -DCMAKE_C_FLAGS="-pipe -O2" \
  ..

# Check if configuration succeeded
if [ $? -ne 0 ]; then
  echo "CMake configuration failed!"
  exit 1
fi

# Build with all available cores
echo "Building with Ninja..."
ninja -j$(nproc)

# Check if build succeeded
if [ $? -ne 0 ]; then
  echo "Build failed!"
  exit 1
fi

# Return to project root to check for binaries
cd ..

# Check for binaries in the correct location (project root bin directory)
if [ -f "bin/snapclient" ] && [ -f "bin/snapserver" ]; then
  echo "Build successful! Binaries are in bin/"
  ls -la bin/
else
  echo "Binaries not found. Check build output for errors."
  ls -la bin/ 2>/dev/null || echo "bin/ directory does not exist"
fi

# Show ccache statistics
if command -v ccache &>/dev/null; then
  echo -e "\nCCache Statistics:"
  ccache -s
fi

echo -e "\nBuild completed at $(date)"
