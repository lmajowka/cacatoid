cmake_minimum_required(VERSION 3.22.1)

project("cacatoid")

# ---------------------------------------------------------------------------
# libsecp256k1 (bitcoin-core) via FetchContent
# ---------------------------------------------------------------------------
include(FetchContent)

# Configure the secp256k1 build before it is made available.
set(SECP256K1_BUILD_BENCHMARK OFF CACHE BOOL "" FORCE)
set(SECP256K1_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(SECP256K1_BUILD_EXHAUSTIVE_TESTS OFF CACHE BOOL "" FORCE)
set(SECP256K1_BUILD_CTIME_TESTS OFF CACHE BOOL "" FORCE)
set(SECP256K1_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(SECP256K1_INSTALL OFF CACHE BOOL "" FORCE)
set(SECP256K1_DISABLE_SHARED ON CACHE BOOL "" FORCE)

FetchContent_Declare(
        secp256k1
        GIT_REPOSITORY https://github.com/bitcoin-core/secp256k1.git
        GIT_TAG v0.5.1
)
FetchContent_MakeAvailable(secp256k1)

# ---------------------------------------------------------------------------
# Native searcher library (loaded by System.loadLibrary("cacatoid"))
# ---------------------------------------------------------------------------
add_library(${CMAKE_PROJECT_NAME} SHARED
        bitcoin_searcher.cpp
        secp256k1_utils.cpp
        sha256.cpp
        ripemd160.cpp
        base58.cpp)

target_compile_features(${CMAKE_PROJECT_NAME} PRIVATE cxx_std_17)
target_compile_options(${CMAKE_PROJECT_NAME} PRIVATE -O3 -funroll-loops)

target_link_libraries(${CMAKE_PROJECT_NAME}
        secp256k1
        android
        log)
