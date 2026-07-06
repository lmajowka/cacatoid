#!/usr/bin/env bash
# Builds libsecp256k1.a for iOS arm64 and copies it to ios/lib/.
# Run once before opening the Xcode project.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
DEPS_DIR="$SCRIPT_DIR/deps"
LIB_DIR="$SCRIPT_DIR/lib"
INC_DIR="$SCRIPT_DIR/include"

mkdir -p "$DEPS_DIR" "$LIB_DIR" "$INC_DIR"

# Clone secp256k1 v0.5.1 if not present.
if [ ! -d "$DEPS_DIR/secp256k1" ]; then
    git clone --branch v0.5.1 --depth 1 \
        https://github.com/bitcoin-core/secp256k1.git \
        "$DEPS_DIR/secp256k1"
fi

BUILD_DIR="$DEPS_DIR/secp256k1/build-ios"
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

cmake .. \
    -DCMAKE_SYSTEM_NAME=iOS \
    -DCMAKE_OSX_ARCHITECTURES=arm64 \
    -DCMAKE_OSX_DEPLOYMENT_TARGET=15.0 \
    -DCMAKE_BUILD_TYPE=Release \
    -DSECP256K1_BUILD_BENCHMARK=OFF \
    -DSECP256K1_BUILD_TESTS=OFF \
    -DSECP256K1_BUILD_EXHAUSTIVE_TESTS=OFF \
    -DSECP256K1_BUILD_CTIME_TESTS=OFF \
    -DSECP256K1_BUILD_EXAMPLES=OFF \
    -DSECP256K1_INSTALL=OFF \
    -DSECP256K1_DISABLE_SHARED=ON

cmake --build . --config Release

cp src/libsecp256k1.a "$LIB_DIR/libsecp256k1.a"
cp "$DEPS_DIR/secp256k1/include/secp256k1.h" "$INC_DIR/secp256k1.h"

echo "Done — libsecp256k1.a ready at ios/lib/"
