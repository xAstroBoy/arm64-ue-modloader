#!/bin/bash
# Build script for Linux/macOS (CI and local development)
# Requires: Android NDK r23c, CMake 3.22+, Ninja

set -e

# ── NDK Detection ─────────────────────────────────────────────────────
if [ -z "$NDK" ]; then
    # Try common paths
    if [ -n "$ANDROID_HOME" ] && [ -d "$ANDROID_HOME/ndk/23.1.7779620" ]; then
        NDK="$ANDROID_HOME/ndk/23.1.7779620"
    elif [ -d "$HOME/Android/Sdk/ndk/23.1.7779620" ]; then
        NDK="$HOME/Android/Sdk/ndk/23.1.7779620"
    elif [ -d "/usr/local/lib/android/sdk/ndk/23.1.7779620" ]; then
        NDK="/usr/local/lib/android/sdk/ndk/23.1.7779620"
    else
        echo "ERROR: Android NDK not found. Set NDK or ANDROID_HOME environment variable."
        echo "  export NDK=/path/to/android/ndk/23.1.7779620"
        exit 1
    fi
fi

echo "Using NDK: $NDK"

ABI="arm64-v8a"
API=24
TOOLCHAIN="$NDK/build/cmake/android.toolchain.cmake"

if [ ! -f "$TOOLCHAIN" ]; then
    echo "ERROR: Toolchain not found at $TOOLCHAIN"
    exit 1
fi

# ── Build ─────────────────────────────────────────────────────────────
mkdir -p build
cd build

cmake -G Ninja \
    -DCMAKE_TOOLCHAIN_FILE="$TOOLCHAIN" \
    -DANDROID_ABI="$ABI" \
    -DANDROID_PLATFORM="android-$API" \
    -DANDROID_STL=c++_static \
    -DCMAKE_BUILD_TYPE=Release \
    ..

ninja -j$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)

echo ""
echo "=== BUILD SUCCEEDED ==="
echo "Output: $(pwd)/libmodloader.so"
ls -la libmodloader.so
