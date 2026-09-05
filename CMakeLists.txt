cmake_minimum_required(VERSION 3.20)
project(VortexPricer VERSION 1.0 LANGUAGES CXX)

# ---------------------------------------------------------
# 1. Standard Enforcement
# ---------------------------------------------------------

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
# Disable compiler-specific extensions (e.g., gnu++23) for pure standard compliance
set(CMAKE_CXX_EXTENSIONS OFF)

# ---------------------------------------------------------
# 2. Performance & Warning Flags
# ---------------------------------------------------------
# We want maximum optimization (-O3) and CPU-specific vectorization (-march=native)
# We also turn on aggressive warnings to catch memory leaks and undefined behavior early
set(CMAKE_CXX_FLAGS "-Wall -Wextra -Wpedantic -Wshadow -O3 -march=native -flto")

# ---------------------------------------------------------
# 3. Directory Structure
# ---------------------------------------------------------
# Tell CMake where to look for your header files
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

# ---------------------------------------------------------
# 4. Target Generation
# ---------------------------------------------------------
# Gather all source files
set(SOURCES
    src/main.cpp
    src/BlackScholes.cpp
    src/MarketData.cpp
)

# Create the executable
add_executable(vortex_engine ${SOURCES})

# Explicitly link standard math libraries (required on some Unix systems)
target_link_libraries(vortex_engine m)